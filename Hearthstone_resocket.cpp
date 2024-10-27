#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <Psapi.h>
using namespace std;
// Need to link with Iphlpapi.lib and Ws2_32.lib
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "ntdll.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
/* Note: could also use malloc() and free() */


char* GetProcessName(DWORD dwPid,  char* szModuleName,DWORD buffsize)
{

    if (!szModuleName || !buffsize) return 0;
    HANDLE pro_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, dwPid);
    if ((INT64)pro_handle > 0)
    {
        HMODULE hMods = 0;;
        DWORD cbNeeded = 0;
        if(EnumProcessModulesEx(pro_handle, &hMods, 8, &cbNeeded, LIST_MODULES_ALL)) //http://msdn.microsoft.com/en-us/library/ms682633(v=vs.85).aspx
        {
            GetModuleFileNameExA(pro_handle, hMods, szModuleName, buffsize);
            return strrchr(szModuleName, '\\');
        }
        CloseHandle(pro_handle);
       
    }
   
    
    return 0;
}


int get_list_Hearthstone( vector<MIB_TCPROW2>* Hearthstone_list)
{
    const char* g_Hearthstonename = "\\Hearthstone.exe";
    Hearthstone_list->resize(0);


    //代码来自 https://docs.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-gettcptable2
    // Declare and initialize variables
    PMIB_TCPTABLE2 pTcpTable;
    ULONG ulSize = 0;
    DWORD dwRetVal = 0;

    char szLocalAddr[128];
    char szRemoteAddr[128];

    struct in_addr IpAddr;

    pTcpTable = (MIB_TCPTABLE2*)MALLOC(sizeof(MIB_TCPTABLE2));
    if (pTcpTable == NULL)
    {
        printf("Error allocating memory\n");
        return 1;
    }

    ulSize = sizeof(MIB_TCPTABLE);
    // Make an initial call to GetTcpTable2 to
    // get the necessary size into the ulSize variable
    if ((dwRetVal = GetTcpTable2(pTcpTable, &ulSize, TRUE)) == ERROR_INSUFFICIENT_BUFFER)
    {
        FREE(pTcpTable);
        pTcpTable = (MIB_TCPTABLE2*)MALLOC(ulSize);
        if (pTcpTable == NULL)
        {
            printf("Error allocating memory\n");
            return 1;
        }
    }
    // Make a second call to GetTcpTable2 to get
    // 
    // the actual data we require
    string socketstate;

    if ((dwRetVal = GetTcpTable2(pTcpTable, &ulSize, TRUE)) == NO_ERROR)
    {
        printf("PID     \t\tLocalAddr:Portt\t\tRemoteAddr:Port        \tstate\n");
        for (int i = 0; i < (int)pTcpTable->dwNumEntries; i++)
        {
            if (MIB_TCP_STATE_ESTAB != pTcpTable->table[i].dwState)//过滤不是连接状态的连接
            {
                continue;
            }

            char processname[256];
            char* filename = GetProcessName(pTcpTable->table[i].dwOwningPid, processname, 256);

            if (!filename)
            {
                continue;
            }
            if ( _stricmp(g_Hearthstonename, filename) != 0)//过滤进程
            {
                continue;
            }
            if (pTcpTable->table[i].dwLocalAddr == 0 || pTcpTable->table[i].dwLocalAddr == 0x100007F)//过滤本地连接
            {
                continue;
            }
            if (pTcpTable->table[i].dwRemoteAddr == 0 || pTcpTable->table[i].dwRemoteAddr == 0x100007F)//过滤本地连接
            {
                continue;
            }
            IpAddr.S_un.S_addr = (u_long)pTcpTable->table[i].dwLocalAddr;
            InetNtopA(AF_INET, &IpAddr, szLocalAddr, 128);
            DWORD dwLocalPort = ntohs(pTcpTable->table[i].dwLocalPort);
            IpAddr.S_un.S_addr = (u_long)pTcpTable->table[i].dwRemoteAddr;
            InetNtopA(AF_INET, &IpAddr, szRemoteAddr, 128);
            DWORD dwRemotePort = ntohs(pTcpTable->table[i].dwRemotePort);
            socketstate = "[" + to_string(pTcpTable->table[i].dwOwningPid) + "]" + string(filename + 1) + "\t";
            socketstate = socketstate + string(szLocalAddr) + ":" + to_string(dwLocalPort) + "\t";
            socketstate = socketstate + string(szRemoteAddr) + ":" + to_string(dwRemotePort) + "\t";
            socketstate = socketstate + "[" + to_string(pTcpTable->table[i].dwState) + "]\n";
            if (dwRemotePort == 443 || dwRemotePort == 80)//过滤掉443端口
            {
                continue;
            }

            printf(socketstate.c_str());

        
            if (_stricmp(g_Hearthstonename, filename) == 0)
            {
                Hearthstone_list->push_back(pTcpTable->table[i]);
            }


        }
    }
    else {
        printf("\tGetTcpTable2 failed with %d\n", dwRetVal);
        FREE(pTcpTable);
        return 1;
    }

    if (pTcpTable != NULL) {
        FREE(pTcpTable);
        pTcpTable = NULL;
    }
    return 0;
}
DWORD thread_resocket(PVOID param)
{
    //vector<MIB_TCPROW2> g_battlenet;//战网网络连接
    vector<MIB_TCPROW2> g_Hearthstone;//炉石传说网络连接
    do
    {

        if (GetAsyncKeyState(VK_RCONTROL) && (GetAsyncKeyState(VK_DELETE)&1))
        {
            
            printf("正在拔线....\n");
            get_list_Hearthstone( &g_Hearthstone);
     
            int count = g_Hearthstone.size();
            if (count > 1)
            {

                MIB_TCPROW* closeSocket = (MIB_TCPROW*)&g_Hearthstone[count - 1];
                closeSocket->dwState = MIB_TCP_STATE_DELETE_TCB;//重置网络连接

                char szLocalAddr[128];
                char szRemoteAddr[128];
                InetNtopA(AF_INET, &closeSocket->dwLocalAddr, szLocalAddr, 128);
                DWORD dwLocalPort = ntohs(closeSocket->dwLocalPort);

                InetNtopA(AF_INET, &closeSocket->dwRemoteAddr, szRemoteAddr, 128);
                DWORD dwRemotePort = ntohs(closeSocket->dwRemotePort);
                printf("已经断开连接--->%s:%d ---- %s:%d \n", szLocalAddr, dwLocalPort, szRemoteAddr, dwRemotePort);
                SetTcpEntry(closeSocket);

                Beep(880, 50);
            }
            
         
            Sleep(20);
        }
       
        
        
        Sleep(10);
    } while (true);
    
    
    return 0;
}


int main()
{

    CreateThread(0, 0, thread_resocket, 0, 0, 0);
   
    printf("启动成功\n[炉石传说]游戏中,按下热键[Ctrl+Delete]拔线\n");
    getchar();
 
    return 0;
}