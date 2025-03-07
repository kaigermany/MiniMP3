
#include <Arduino.h>
#ifndef LinkedListClass
#define LinkedListClass

typedef struct LinkedListEntryStruct{
	void* object;
	struct LinkedListEntryStruct* next;
} LinkedListEntry;

class LinkedList{
	public:
		int size;
		LinkedListEntry* firstEntry;
		LinkedListEntry* lastEntry;
		
		LinkedList();
		~LinkedList();
		
		void clear();
		void removeEntry(void* obj);
		bool addEntry(void* obj);
		void* removeFirstEntry();
		void* getEntry(int index);
};
#endif