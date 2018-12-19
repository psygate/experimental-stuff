#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define SIGNATURE 0x1122334455667788
#define SIGNATURE_PTR (void *)SIGNATURE

/*
Stateful accumulation function in C. Do _not_ really use this anywhere.
*/

typedef struct AccumulatorSpace
{
	int init;
	int *scratch_pad;
	int *scratch_pad_base_pointer;
	void *call_target;
	void *call_target_base_pointer;
} AccumulatorSpace;

unsigned char NOP = 0x90;
unsigned char RET = 0xC3;

int accumulator(int* tgt, int value)
{
	*tgt += value;
	return *tgt;
}

int callwrapper(int value)
{
	int* tgt = SIGNATURE_PTR;
	*tgt += value;
	return *tgt;
}

void* allocate_page()
{
	return mmap(
					NULL, 
					getpagesize(), 
					PROT_EXEC | PROT_READ | PROT_WRITE, 
					MAP_PRIVATE | MAP_ANONYMOUS, 
					-1, 
					0
				);
}

void* get_accumulator()
{
	static struct AccumulatorSpace space = {0, NULL, NULL, NULL, NULL};
	
	if(!space.init)
	{
		printf("Initializing AccumulatorSpace.\n");
		space.init = ~space.init;
		space.scratch_pad = allocate_page();
		space.call_target = allocate_page();
		space.scratch_pad_base_pointer = space.scratch_pad;
		space.call_target_base_pointer = space.call_target;
	}
	
	printf("AccumulatorSpace(%i, %p, %p, %p, %p)\n", space.init, space.scratch_pad, space.call_target, space.scratch_pad_base_pointer, space.call_target_base_pointer);

	// Allocate the call wrapper.
	int *local_scratch_pad = space.scratch_pad++;
	unsigned char *call_target_pad = (unsigned char *) space.call_target;
	unsigned char *callwrapper_ptr = (unsigned char *) callwrapper;

	// Copy the call wraper bytecode (until we hit a RETURN instruction.)
	size_t i;
	for(i = 0;; i++)
	{
		call_target_pad[i] = callwrapper_ptr[i];
		if(call_target_pad[i] == RET) break;
	}

	printf("Copied function. (%zi bytes).\n", i);
	int signature_found = 0;

	//Insert the accumulation scratch pad space.
	for(size_t j = 0; j < i + sizeof(int); j++)
	{
		int **window = (int **) (call_target_pad + j);
		
		if(*window == SIGNATURE_PTR)
		{
			printf("Replacing SIGNATURE. (window_ptr: %p, window_value: %p)\n", window, *window);
			*window = local_scratch_pad;
			**window = 0;

			printf("Replaced SIGNATURE. (window_ptr: %p, window_value: %p, int: %i)\n", window, *window, **window);

			signature_found = !signature_found;

			break;
		}
	}

	if(!signature_found)
	{
		printf("Failed to find scratch pad pointer.\n");
		return NULL;
	}
	else
	{
		space.scratch_pad += 1;
		space.call_target += i + 1;
		printf("Allocated accumulator.\n");
		return call_target_pad;
	}
}

void main(void)
{
	int (*x)(int) = get_accumulator();
	if(x != NULL)
	{
		printf("----- X -----\n");
		printf("Accumulating 1:  %i\n", x(1));
		printf("Accumulating 10: %i\n", x(10));
		printf("Accumulating 23: %i\n", x(23));
		printf("Accumulating 3:  %i\n", x(3));
	}
	else
	{
		printf("Can't allocate accumulator x.\n");
	}

	int (*y)(int) = get_accumulator();
	if(y != NULL)
	{
		printf("----- Y -----\n");
		printf("Accumulating 1:  %i\n", y(1));
		printf("Accumulating 10: %i\n", y(2));
		printf("Accumulating 23: %i\n", y(3));
		printf("Accumulating 3:  %i\n", y(4));
		printf("----- X -----\n");
		printf("Accumulating 1:  %i\n", x(1));
		printf("Accumulating 10: %i\n", x(10));
		printf("Accumulating 23: %i\n", x(23));
		printf("Accumulating 3:  %i\n", x(3));
	}
	else
	{
		printf("Can't allocate accumulator y.\n");
	}
}