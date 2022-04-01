#include "mmu.cpp"

int main() {
    GC_ENABLE = false;

    mmu_t* mmu;
    size_t size = 250<<20;
    mmu = create_mem(mmu, size);

    addrs t1 = create_var(mmu, INT);

    int value = 100;
    assign_var(mmu, t1, INT, value);

    int val = getval(mmu, t1);
    cout<<"Value is : "<<val<<endl;

    addrs t2 = create_var(mmu, BOOL);

    return 0;
}