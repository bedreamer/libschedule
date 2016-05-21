#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_
/*
 *  序列化发送队列的优先级排序处理模块，用于半双工总线设备轮询通讯

优先级	描述
-2	最低，每轮训4个循环执行一次
-1	较低，每轮询2个循环执行一次
0	普通，顺序执行
1	较高，立即执行一次
2	较实时，立即穿插执行，并周期执行
3	实时，立即全速周期执行
*/
#define MAX_NODE    64

struct schedule_node {
    // 当前执行的循环次数
    int nr_loop;

    // 默认优先级
    int default_lv;
    // 前一优先级
    int prev_lv;
    // 当前优先级
    int current_lv;

    // 私有参数
    void *some_param;
    // 序列节点, 用于搜索, 编辑
    struct schedule_node *permanent_next;
};

struct schedule_struct {
    // 执行序列中的节点个数
    unsigned int nr_node;
    // 当前执行的循环次数
    int nr_loop;
	// 需要移动游标
	int need_move_cursor;

    // 当前执行序列列表
    struct schedule_node *schedule_head[512];
    // 执行序列中的节点个数
    int nr_seq;
    // 当前执行的节点
    int schedule_now;

    // 所有执行序列
    struct schedule_node *permanent_heads;
    // 私有参数
    void *some_param;
};


// 创建一个新的计划
struct schedule_struct *schedule_create(void *some_param);
// 注册一个执行序列
struct schedule_node *sechedule_register(struct schedule_struct *, int lv, void *some_param);

// 获取下一个执行序列节点
struct schedule_node *schedule_next(struct schedule_struct *scheduler);

#endif // _SCHEDULE_H_
