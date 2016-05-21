#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS
    #include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "schedule.h"

// 一次性返回优先级发生变化的所有节点
static int schedule_level_changed(struct schedule_struct *s)
{
    struct schedule_node *n;
    int changed = 0;
    if ( ! s ) return NULL;

    n = s->permanent_heads;
    while (n) {
        if ( n->current_lv != n->prev_lv ) {
			n->prev_lv = n->current_lv;
			changed ++;
        }
        n = n->permanent_next;
    }

    return changed;
}

// 计算该级别下的节点个数
static int schedule_count(struct schedule_struct *s, int level)
{
    struct schedule_node *n;
    int count = 0;
    if ( ! s ) return NULL;

    n = s->permanent_heads;
    while (n) {
        if ( n->current_lv == level ) {
            count ++;
        }
        n = n->permanent_next;
    }

    return count;
}

// 选择该级别下的节点
static int schedule_select(struct schedule_struct *s, int level, struct schedule_node **wr)
{
    struct schedule_node *n;
    int count = 0;
    if ( ! s ) return NULL;

    n = s->permanent_heads;
    while (n) {
		// 优先级发生变化
		if (n->current_lv == level ) {
			wr[count++] = n;
			if (count >= MAX_NODE) break;
		} else {
		}

        n = n->permanent_next;
    }

    return count;
}

// 重新构造执行序列
static void schedule_rebuild(struct schedule_struct *s)
{
    int nr_P3, nr_P2, nr_P1, nr_P0, nr_Pn1, nr_Pn2;
    struct schedule_node *P3[MAX_NODE], *P2[MAX_NODE], *P1[MAX_NODE], *P0[MAX_NODE];
    struct schedule_node *Pn1[MAX_NODE], *Pn2[MAX_NODE];

    // 删除当前执行序列
    s->nr_seq = 0;
    s->schedule_now = 0;
    memset(s->schedule_head, 0, sizeof(s->schedule_head));

    nr_P3 = schedule_select(s, 3, P3);
    if ( nr_P3 ) {
        memcpy(s->schedule_head, P3, nr_P3 * sizeof(struct schedule_node *));
        return;
    }

    // 计算个数
    nr_P2 = schedule_select(s, 2, P2);
    nr_P1 = schedule_select(s, 1, P1);
    nr_P0 = schedule_select(s, 0, P0);
    nr_Pn1 = schedule_select(s, -1, Pn1);
    nr_Pn2 = schedule_select(s, -2, Pn2);

    // 将优先级为2的节点插入新的执行序列
    if ( nr_P2 ) {
        int repeat = nr_P1 + nr_P0 + nr_Pn1 + nr_Pn2;
        int i;
		if (repeat == 0)
			repeat = 1;
        for ( i = 0; i < repeat; i ++ ) {
            int j;
            for ( j = 0; j < nr_P2; j ++ ) {
                s->schedule_head[ i * nr_P2 + j + i ] = P2[j];
                s->nr_seq ++;
            }
        }
    }

    // 将优先级1的点插入新的执行序列
    if ( nr_P1 ) {
        int i = 0;
        for ( i = 0; i < nr_P1; i ++ ) {
            s->schedule_head[ i * nr_P2 + nr_P2 + i ] = P1[i];
			s->nr_seq ++;
        }
    }

    // 将优先级为0的点插入新的执行序列
    if ( nr_P0 ) {
        int base = nr_P2 * nr_P1 + nr_P1;
        int j = 0;

        for ( j = 0; j < nr_P0; j ++ ) {
            s->schedule_head[ base + j * nr_P2 + nr_P2 + j  ] = P0[j];
			s->nr_seq ++;
        }
    }

    // 将优先级为-1的点插入新的执行序列
    if ( nr_Pn1 ) {
        int base = nr_P2 * (nr_P1 + nr_P0 ) + nr_P1 + nr_P0;
        int j = 0;

        for ( j = 0; j < nr_Pn1; j ++ ) {
            s->schedule_head[ base + j * nr_P2 + nr_P2 + j  ] = Pn1[j];
			s->nr_seq ++;
        }
    }

    // 将优先级为-2的点插入新的执行序列
    if ( nr_Pn2 ) {
		int base = nr_P2 * (nr_P1 + nr_P0 + nr_Pn1) + nr_P1 + nr_P0 + nr_Pn1;
        int j = 0;

        for ( j = 0; j < nr_Pn2; j ++ ) {
			s->schedule_head[ base + j * nr_P2 + nr_P2 + j  ] = Pn2[j];
            s->nr_seq ++;
        }
    }
}

/* 序列中有节点的优先级发生变化后需要重新生成执行序列, 否则返回当前序列中的对应
 *
 * 若出现优先级变化做如下处理来重新生成执行序列：
 *      1. 将执行序列中的节点删除
 *      2. 从所有执行序列中找到优先级为P3, P2, P1, P0, Pn1, Pn2的可执行节点，分别记为列表P3, P2, P1, P0, Pn1, Pn2
 *      3. 若有P3优先级的节点，则直接使用P3替换当前执行列表，并返回
 *      4. 将P0优先级的节点插入当前执行列表
 *      5. 将P1优先级的节点插入当前执行列表的头部
 *      7. 将Pn1中节点插入当前执行列表的尾部
 *      8. 将Pn2中的节点插入当前执行列表的尾部
 *      6. 将P2优先级的节点间隔插入当前执行列表的头部
 *      9. 将schedule_now指向当前列表头部
 *      10. 返回schedule_now指向的节点
 * 若没有出现优先级变化：
 *  直接返回schedule_now指向的节点
 */
struct schedule_node *schedule_next(struct schedule_struct *s)
{
    struct schedule_node *schedule_next;
    if ( !s ) return NULL;

	if (s->schedule_now >= s->nr_seq) {
		s->schedule_now = 0;
		s->nr_loop++;
	}

	if ( schedule_level_changed(s) || s->schedule_now >= s->nr_seq ) {
		schedule_rebuild(s);
		schedule_next = s->schedule_head[s->schedule_now];
		schedule_next->nr_loop = s->nr_loop;
		s->schedule_now++;
		s->need_move_cursor ++;

		return schedule_next;
	}

	while (1) {
		if (s->schedule_now >= s->nr_seq) {
			s->schedule_now = 0;
			s->nr_loop++;
		}
		schedule_next = s->schedule_head[s->schedule_now];
		s->schedule_now++;
		if (schedule_next->current_lv == -1 && s->nr_loop % 3) {
			continue;
		} else if (schedule_next->current_lv == -2 && s->nr_loop % 5) {
			continue;
		} else {
			break;
		}
	}
	schedule_next->nr_loop = s->nr_loop;

	return schedule_next;
}

// 创建一个新的计划
struct schedule_struct *schedule_create(void *some_param)
{
    struct schedule_struct *s = (struct schedule_struct*)malloc(sizeof(struct schedule_struct));
    if ( ! s ) return NULL;

    memset(s, 0, sizeof(struct schedule_struct));
    s->some_param = some_param;

    return s;
}

// 注册一个执行序列
struct schedule_node *sechedule_register(struct schedule_struct *s, int lv, void *some_param)
{
    struct schedule_node *n = NULL;
    struct schedule_node **thiz = NULL;
    if ( ! s ) return NULL;

    n = (struct schedule_node*)malloc(sizeof(struct schedule_node));
    memset(n, 0, sizeof(struct schedule_node));
    n->default_lv = lv;
    n->current_lv = lv;
    n->some_param = some_param;
	n->nr_loop = -1;

    thiz = &s->permanent_heads;
    while ( *thiz ) {
        thiz = &(*thiz)->permanent_next;
    }
    *thiz = n;
    s->nr_node ++;

    return n;
}
#if 0
int main()
{
	struct schedule_struct *root = schedule_create(NULL);
	sechedule_register(root, 0, 1);

	sechedule_register(root, 0, 2);
	sechedule_register(root, 0, 3);
	sechedule_register(root, 0, 4);
	sechedule_register(root, 0, 5);
	sechedule_register(root, 0, 6);
	sechedule_register(root, 0, 7);
	sechedule_register(root, 0, 8);
	sechedule_register(root, 0, 9);
	sechedule_register(root, 0, 10);

	int id, ni;

	while (1) {
		struct schedule_node* n = schedule_next(root);
		int i = 0;
		system("cls");
		printf("\r=====loop: %d======now: %d===========\n", root->nr_loop, n->some_param);
		printf(" %6s%6s%6s\n", "ID", "NI", "loop");
		for (i = 0; i < root->nr_seq; i++) {
			printf("%C%6d%s%6d%6d\n", 
				i + 1 == root->schedule_head[i]->some_param && n->some_param == root->schedule_head[i]->some_param ? '#':' ',
				root->schedule_head[i]->some_param,
				root->schedule_head[i]->current_lv==2?"*":" ",
				root->schedule_head[i]->current_lv,
				root->schedule_head[i]->nr_loop);
		}
		printf("input: ");
		id = getchar();
		if (id == 0x0a) continue;
		scanf("%d%d", &id, &ni);
		{
			struct schedule_node *n;
			int count = 0;

			n = root->permanent_heads;
			while (n) {
				if (n->some_param == id) {
					n->current_lv = ni;
					break;
				}
				n = n->permanent_next;
			}
		}
	}
}
#endif
