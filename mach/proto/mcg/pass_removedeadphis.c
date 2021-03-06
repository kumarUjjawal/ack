#include "mcg.h"

static ARRAYOF(struct ir) phis;
static bool changed;

static void collect_phis(struct basicblock* bb)
{
    int i;

    for (i=0; i<bb->irs.count; i++)
    {
        struct ir* ir = bb->irs.item[i];
        if (ir->opcode == IR_PHI)
            array_append(&phis, ir);
    }
}

static bool ir_walker_cb(struct ir* ir, void* user)
{
    if (ir->left)
        array_remove(&phis, ir->left);
    if (ir->right)
        array_remove(&phis, ir->right);

    return false;
}

static void remove_referenced_phis(struct basicblock* bb)
{
    int i, j;

    for (i=0; i<bb->irs.count; i++)
    {
        struct ir* ir = bb->irs.item[i];
        switch (ir->opcode)
        {
            case IR_PHI:
                for (j=0; j<ir->u.phivalue.count; j++)
                    array_remove(&phis, ir->u.phivalue.item[j].right);
                break;

            default:
                ir_walk(ir, ir_walker_cb, NULL);
                break;
        }
    }
}

static void purge_unused_phis(struct basicblock* bb)
{
    int i;
    
    for (i=0; i<bb->irs.count; i++)
    {
        struct ir* ir = bb->irs.item[i];
        if ((ir->opcode == IR_PHI) && (array_contains(&phis, ir)))
        {
            array_remove(&bb->irs, ir);
            i--;
            changed = true;
        }
    }
}

void pass_remove_dead_phis(void)
{
    int i;
    
    do
    {
        changed = false;

        phis.count = 0;
        for (i=0; i<cfg.preorder.count; i++)
            collect_phis(cfg.preorder.item[i]);

        for (i=0; i<cfg.preorder.count; i++)
            remove_referenced_phis(cfg.preorder.item[i]);

        for (i=0; i<cfg.preorder.count; i++)
            purge_unused_phis(cfg.preorder.item[i]);
    }
    while (changed);
}

/* vim: set sw=4 ts=4 expandtab : */


