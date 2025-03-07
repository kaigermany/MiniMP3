
#ifndef ReadableBlock_H
#define ReadableBlock_H
class ReadableBlock{
	private:
		
	public:
		char* buf;
		int off;
		int len;
		int offsetRestore;
		
		ReadableBlock(char* buf, int off, int len);
		~ReadableBlock();
};
#endif
