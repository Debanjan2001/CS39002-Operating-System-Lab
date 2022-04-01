#include<iostream>
using namespace std;

#include "mmu.cpp"

mmu_t *my_memory;
const size_t megabytes = (1<<20);

void f1(addrs x, addrs y){
    // Not sure about x,y
    if(GC_ENABLE) push_rbp();
    addrs myarr = create_arr(my_memory, MED_INT, 50000);
    int value = 20;
    assign_arr(my_memory, myarr, MED_INT, value);
    
    if(GC_ENABLE) gc_pop_frame(my_memory);
    else free_elem(my_memory, myarr);
}

int main(){
    ANALYSE = true;
    GC_ENABLE = false;
    size_t required = ( megabytes * 500 ); 
    my_memory = create_mem(my_memory, required);

    addrs var1, var2;    
    var1 = create_var(my_memory, MED_INT);
    var2 = create_var(my_memory, MED_INT);

    int value1 = 20, value2 = 9100;
    assign_var(my_memory, var1, MED_INT, value1);
    assign_var(my_memory, var2, MED_INT, value2);

    for(int i=0;i<10;i++){
        f1(var1,var2);
    }
    if(GC_ENABLE) gc_pop_frame(my_memory);
    // if(GC_ENABLE) gc_run(my_memory);
    return 0;
}