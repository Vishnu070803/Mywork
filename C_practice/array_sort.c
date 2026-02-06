#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
void swap(int a[], int i){
    int temp = a[i];
    a[i] = a[i + 1];
    a[i + 1] = temp;
    return;
}
void swap_2(int a[], int i, int j){
    int temp = a[i];
    a[i] = a[j];
    a[j] = temp;
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
        // n -1 - i because after each outer loop last element will be sorted
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
        // we assume the i th element as min and look into the array if any number less than the i th element we swap the places
        for (int j = i + 1; j < n; j++){
            if(a[j] < a[minimum_index]){
                minimum_index = j;
                swapped = true;
            }
        }
        if(swapped){
            swap_2(a, i, minimum_index);
            // Swap the found minimum element with the current element
        }
    }
}
void Insertion_sort(int a[], int n){
    int i, j;
    for(i = 1; i <= n - 1; i++){
        j = i - 1;
        /* while loop to check whether the current element is greater than the next 
        element and repeat these until all previous elements compared */
        while (j >= 0 && a[j] > a[ j + 1])
        {
            swap(a, j);
            j--;
        }
    }
}
void insertion_sort_perfect(int a[], int n)
{
    int i, j, key;

    for (i = 1; i < n; i++)
    {
        key = a[i];      // element to insert
        j = i - 1;

        /* Shift elements greater than key to one position ahead */
        while (j >= 0 && a[j] > key)
        {
            a[j + 1] = a[j];
            j--;
        }

        a[j + 1] = key;  // insert key in correct place
    }
}

void merge(int a[], int low, int mid, int high){
    int size = (high - low ) + 1;
    int temp[size];
    int i = 0;
    int left = low;
    int right = (mid + 1);
    while (left <= mid && right <= high)
    {
        if(a[left] <= a[right]){
            temp[i]= a[left];
            left++;
            i++;
        }else{
            temp[i] = a[right];
            right++;
            i++;
        }
    }
    while (left <= mid)
    {
        temp[i] = a[left];
        left++;
        i++;
    }
    while (right <= high)
    {
        temp[i] = a[right];
        right++;
        i++;
    }
    for(i = low; i <= high; i++){
        a[i] = temp[i - low];
    }
    return;
}
void merge_sort(int a[], int low, int high){
    if(low >= high)
        return;
    int mid = low + (high - low) / 2 ;
    merge_sort(a, low, mid);
    merge_sort(a, (mid + 1), high);
    merge(a, low, mid, high);
    return;
}

int partition(int a[], int low, int high){
    int i = low;
    int j = high;
    int pivot = low; // pivot is the first element of the array
    // We are using the first element as pivot
    // If we want to use last element as pivot then we can change the pivot to high
    while(i < j){
    // i scans from the left for elements greater than the pivot
    // j scans from the right for elements less than or equal to the pivot
    // When i < j & a=i at greater and j at smaller elements than pivot, swap a[i] and a[j] to move misplaced elements to correct sides
        while (a[i] <= a[pivot] && i <= (high - 1))
        {
            i++;
        }
        while (a[j] > a[pivot] && j >= (low + 1))
        {
            j--;
        }
        if(i < j){
            swap_2(a, i, j);
        }
    }
    swap_2(a, pivot, j);
    return j;
}
void Quick_sort(int a[], int low, int high){
    if(low < high){
        int pivot_index = partition(a, low, high);
        Quick_sort(a, low, (pivot_index - 1 ));
        Quick_sort(a, (pivot_index + 1), high);
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
   // Selection_sort(a, n);
   // Insertion_sort(a, n);
   // merge_sort(a, 0, (n - 1));
    Quick_sort(a, 0, (n - 1));
    printf("Sorted array:\n");
    print_array(a, n);
    return 0;
}
