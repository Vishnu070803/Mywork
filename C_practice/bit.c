
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
int Toggle() {
    unsigned int value = 0b00011111;      // Initial value: 31
    unsigned int bitmask = 0b00010101;    // Toggle bits 0, 2, 4

    printf("Before toggling: %u (binary: %08b)\n", value, value);
    
    value ^= bitmask;  // Toggle bits
    
    printf("After toggling : %u (binary: %08B)\n", value, value);

    return 0;
}
void powerof2(int a){
    if((a != 0) && ((a &((a - 1)) )== 0)){
        printf("%d is a power of 2\n", a);
    }else{
        printf("%d is a not power of 2\n", a);
    }
}
void reversebit(unsigned int a){
    unsigned int reverse = 0;
    for(int i = 0; i < 32; i++){
        reverse = reverse * 2 + (a % 2);
        a = a / 2;
    }
    printf("reversed %x\n", reverse);
}
void check_ith_bit(unsigned int a, int i){
    unsigned temp = (a & (1 << i));
    printf("%b", temp);
}
void swap(int a, int b){
    a = a ^ b;
    b = a ^ b;
    a = a ^ b;
    printf("%d and %d", a, b);
}
void mul(int a, int b){
    if (b <= 0) {
        printf("Input must be positive.\n");
        return;
    }

    int power = 1;
    int shift = 0;

    // Find the highest power of 2 less than or equal to b, and its bit position (shift)
    while ((power << 1) <= b) {
        power <<= 1;
        shift++;
    }

    int remainder = b - power;

    // Shift 'a' by 'shift' instead of 'power' (since shift is bit position)
    int temp = (a << shift) + (a * remainder);

    printf("a = %d, mul (by shift+remainder) = %d\n", a, temp);

    // For comparison
    temp = a * b;
    printf("a = %d, mul (by *) = %d\n", a, temp);

    return;
}

void turn_off_rightmost_bit(int n) {
    int result = n & (n - 1);
    printf("Original       : %d (0x%X)\n", n, n);
    printf("After clearing : %d (0x%X)\n", result, result);
}
void find_two_unique(int arr[], int n, int* x, int* y) {
    int xor_all = 0;

    // Step 1: XOR of all elements = x ^ y
    for (int i = 0; i < n; i++) {
        xor_all ^= arr[i];
    }

    // Step 2: Find rightmost set bit in xor_all
    int set_bit = xor_all & ~(xor_all - 1);  // isolates rightmost set bit

    *x = 0;
    *y = 0;

    // Step 3: Divide elements into two groups and XOR separately
    for (int i = 0; i < n; i++) {
        if (arr[i] & set_bit)
            *x ^= arr[i];
        else
            *y ^= arr[i];
    }
}
int subtract(int a, int b) {
    while (b != 0) {
        int borrow = (~a & b) << 1;
        a = a ^ b;
        b = borrow;
    }
    return a;
}
int add(int a, int b) {
    while (b != 0) {
        int carry = (a & b) << 1;
        a = a ^ b;
        b = carry;
    }
    return a;
}
void fibonaci(int num){
    int a = 0, b = 1, next = 0; 
    for(int i = 0; i < num; i++){
        printf("%d\n", a);
        next = a + b;
        a = b;
        b = next;
    }
}
struct Node {
	int data;
	struct Node* next;
};

int i = 0;

void new_node(struct Node **head) {
	struct Node* new_node = (struct Node *)malloc(sizeof(struct Node));
	new_node->data = ++i;
	new_node->next = NULL;

	if (*head == NULL) {
		*head = new_node;
	} else {
		struct Node* temp = *head;
		while (temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = new_node;
	}
}
void search(struct Node *head, int data) {
	struct Node *temp = head;
	int i = 1;
	while(temp != NULL) {
		if(temp->data == data) {
			printf("Found %d at element %d\n", temp->data, i);
			break;
		}
		temp = temp->next;
		i++;
	}
}
void insertion(struct Node *head, int data, int position) {
	struct Node *temp = head;
	int i = 1;
	while(temp != NULL) {
		if(position == i) {
			temp->data = data;
			break;
		}
		temp = temp->next;
		i++;
	}
}
void print_list(struct Node *head) {
	struct Node *temp = head;
	while(temp != NULL) {
		printf("%d->", temp->data);
		temp = temp->next;
	}
	printf("tail\n");
}
void reverse_ll(struct Node **head) {
	struct Node* prev = NULL;
	struct Node* cur = *head;
	struct Node* next = NULL;
	while(cur != NULL) {
		next = cur->next;
		cur->next = prev;
		prev = cur;
		cur = next;
	}
	*head = prev;
	print_list(*head);

}
void linked_list(void) {
	struct Node* head = NULL;
	new_node(&head);
	new_node(&head);
	new_node(&head);

	print_list(head);
	search(head, 2);
	insertion(head, 5, 2);
	print_list(head);
	// reverse_ll(&head);
	return;
}


void count_bits(int a) {
	int count = 0;
	for(int i = 0; i < 32 ; i++) { // use i < 32 to stay in bounds
		if(a & (1 << i)) {
			count++;
		}
	}
	printf("count of set bits %d\n", count);
}
void even_odd(int a) {
	if(a & (1)) {
		printf("ODD\n");
	} else if(a && 0) {
		printf("ZERO\n");
	} else {
		printf("EVEN\n");
	}
	return;
}
void nibble_switch(unsigned int a) {
	unsigned int reverse = 0;
	int count = 0;

	while (count < 8) {
		reverse = reverse * 16 + (a % 16);
		a /= 16;
		count++;
	}

	printf("Reversed hex value: 0x%08X\n", reverse);
}

void byte_switch(unsigned int a) {
	unsigned int reverse = 0;

	for (int i = 0; i < 4; i++) {
		int byte = a % 256;          // Get the last byte
		reverse = reverse * 256 + byte; // Shift existing reverse and add new byte
		a = a / 256;                 // Move to next byte
	}

	printf("Reversed (byte order) = %X (0x%08X)\n", reverse, reverse);
}
void print_binary(int a) {
	for(int i = 31; i >= 0; i--) {
		printf("%d", ((a >> i) & (1)));
	}
	printf("\n");
}
void bit_swap(int a) {
	int b = 0;
	int temp = 0;
	for(int i = 31; i >= 0; i--) {
		temp = (a >> i) & (1);
		b = b | (temp <<(31 - i) );
	}
	print_binary(b);
	print_binary(a);
}


int main() {
    byte_switch(0x10000000);
    
	return 0;
}

