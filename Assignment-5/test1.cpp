#include "mmu.cpp"

int main() {
    GC_ENABLE = true;

    mmu_t* mmu;
    size_t size = 250<<20;
    mmu = create_mem(mmu, size);

    addrs t1 = create_var(mmu, INT);

    int value = 100;
    assign_var(mmu, t1, INT, value);

    int val = getval(mmu, t1);
    cout<<"Value is : "<<val<<endl;

    addrs t2 = create_var(mmu, BOOL);

    assign_var(mmu, t2, BOOL, true);

    addrs t3 = create_var(mmu, CHAR);

    assign_var(mmu, t3, CHAR, true);

    addrs t4 = create_var(mmu, MED_INT);

    assign_var(mmu, t4, MED_INT, true);


    return 0;
}