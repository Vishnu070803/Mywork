#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
void swap(int a[], int i){
    int temp = a[i];
    a[i] = a[i + 1];
    a[i + 1] = temp;
    return;
}
void print_array(int a[], int n){
    for (int j = 0; j < n; j++) {
        printf("%d ", a[j]);
    }
    printf("\n");

}
void Bubble_sort(int a[], int n){
    for(int i = 0; i < n -1; i++){
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++){
            if(a[j] > a[j + 1]){
                swap(a, j);
                swapped = 1;
            }
        }
        if(!swapped){
            printf("Sorted in %d stages : \n", i + 1);
            break;
        }
    }
}
void Selection_sort(int a[], int n){
    for(int i = 0; i < n - 1; i++){
        int minimum_index = i;
        bool swapped = false;
        for (int j = i + 1; j < n; j++){
            if(a[j] < a[minimum_index]){
                minimum_index = j;
                swapped = true;
            }
        }
        if(swapped){
            int temp = a[minimum_index];
            a[minimum_index] = a[i];
            a[i] = temp; // Swap the found minimum element with the current element
        }
        
    }
}
int main(void) {
    int n;
    printf("Enter the number of elements in the array:\n");
    scanf("%d", &n);

    int a[n];
    printf("Enter the %d elements to fill array:\n", n);
    for (int j = 0; j < n; j++) {
        scanf("%d", &a[j]);
    }
    printf("Entered array:\n");
    print_array(a, n);
   // Bubble_sort(a, n);
    Selection_sort(a, n);
    printf("Sorted array:\n");
    print_array(a, n);
    return 0;
}