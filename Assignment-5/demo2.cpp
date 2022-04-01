#include<iostream>
#include "mmu.cpp"

using namespace std;

const size_t megabytes = (1<<20);

void fibonacci(addrs arr, int k, mmu_t *my_memory){
    if(GC_ENABLE) push_rbp();
    assign_arr(my_memory, arr, INT, 0, 0);
    
    if(k==1){
        return;
    }

    assign_arr(my_memory, arr, INT, 1, 1);
    for(int i=2;i<k;i++){   
        int fi_1 = getval(my_memory, arr, i-1);
        int fi_2 = getval(my_memory, arr, i-2);
        assign_arr(my_memory, arr, INT, fi_1+fi_2, i);
    }
    if(GC_ENABLE) gc_pop_frame(my_memory);
}

int fibonacciProduct(int k){
    ANALYSE = true;
    GC_ENABLE = false;
    mmu_t *my_memory;
    my_memory = create_mem(my_memory, megabytes*500);

    addrs arr = create_arr(my_memory, INT, k);

    fibonacci(arr, k, my_memory);

    int product=1;
    for(int i=1;i<k;i++){
        product *= getval(my_memory, arr, i);
    }
    if(GC_ENABLE) gc_pop_frame(my_memory);

    return product;
}

int main(){

    int k;
    cout<<">> Enter k: ";
    cin>>k;

    int product = fibonacciProduct(k);

    cout<<"Product is = "<<product<<endl;

    return 0;
}