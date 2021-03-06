/* The objective of this program is to identify read and write instructions in target program 
 * and corresponding accessed memory addresses. 
 * Version 1.0
 */

#include <stdio.h>
#include "pin.H"
using std::string;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "memory_access.log", "specify output file name");

//==============================================================
//  Analysis Routines
//==============================================================
PIN_LOCK pinLock;
FILE *out;

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v){
	PIN_GetLock(&pinLock, threadid+1);
	fprintf(out, "thread begin %d\n", threadid);
	fflush(out);
	PIN_ReleaseLock(&pinLock);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v){
	PIN_GetLock(&pinLock, threadid+1);
	fprintf(out, "thread end %d, code %d\n", threadid, code);
	fflush(out);
	PIN_ReleaseLock(&pinLock);
}

// This routine is executed to check if the instruction writes memory. 
VOID MemoryWrite(VOID *addr, THREADID threadid){
	PIN_GetLock(&pinLock, threadid+1);
	if (threadid == 1 || threadid == 2)
	{
		fprintf(out, "thread id %d, memory write, memory address %p\n", threadid, addr);
		fflush(out);
	}
	PIN_ReleaseLock(&pinLock);
}
 // This routine is executed to check if the instruction reads memory. 
VOID MemoryRead(VOID *addr, THREADID threadid){
	PIN_GetLock(&pinLock, threadid+1);
	if (threadid != 0)
	{
		fprintf(out, "thread id %d, memory read, memory address %p\n", threadid, addr);
		fflush(out);
	}
	PIN_ReleaseLock(&pinLock);
}

//====================================================================
// Instrumentation Routines
//====================================================================

// This routine is executed for each instruction.
VOID Instruction(INS ins, VOID *v){
	if (INS_IsMemoryRead(ins))
	{
		INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(MemoryRead), IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_END);
	}
	if (INS_IsMemoryWrite(ins))
	{
		INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(MemoryWrite), IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_END);
	}
}

// This routine is executed once at the end.
VOID Fini(INT32 code, VOID *v){
	fclose(out);
}

// =====================================================================
// Print Help Message                                                 
// =====================================================================

INT32 Usage(){
	PIN_ERROR("This Pintool identifies each memory write and memory read instruction\n" + KNOB_BASE::StringKnobSummary() + "\n");
	return -1;
}

// =====================================================================
// Main                                                                 
// =====================================================================

int main(int argc, char *argv[])
{
	// Initialize the pin lock
    PIN_InitLock(&pinLock);

    // Initialize pin
    if (PIN_Init(argc, argv)){
    	return Usage();
    }
    PIN_InitSymbols();

	out = fopen(KnobOutputFile.Value().c_str(), "w");

    // Register Analysis routines to be called when a thread begins/ends
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);
	
	PIN_StartProgram();
	return 0;
}