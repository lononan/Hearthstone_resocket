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
vector<MIB_TCPROW2> g_battlenet;//战网网络连接
vector<MIB_TCPROW2> g_Hearthstone;//炉石传说网络连接

const char *g_battlenetname="\\Battle.net.exe", * g_Hearthstonename="\\Hearthstone.exe";
int get_list(vector<MIB_TCPROW2>* battlenet_list, vector<MIB_TCPROW2>* Hearthstone_list)
{
    battlenet_list->resize(0);
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
            if (MIB_TCP_STATE_ESTAB!= pTcpTable->table[i].dwState)//过滤不是连接状态的连接
            {
                continue;
            }
           
            char processname[256];
            char *filename=GetProcessName(pTcpTable->table[i].dwOwningPid, processname, 256);
            
            if (!filename)
            {
                continue;
            }
            if (_stricmp(g_battlenetname, filename)!=0 && _stricmp(g_Hearthstonename, filename) != 0)//过滤进程
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
            socketstate = "["+to_string(pTcpTable->table[i].dwOwningPid)+"]"+ string(filename+1) + "\t";
            socketstate = socketstate + string(szLocalAddr) + ":" + to_string(dwLocalPort) + "\t";
            socketstate = socketstate + string(szRemoteAddr) + ":" + to_string(dwRemotePort) + "\t";
            socketstate = socketstate + "[" + to_string(pTcpTable->table[i].dwState) + "]\n";
            if (dwRemotePort == 443 || dwRemotePort == 80)//过滤掉443端口
            {
                continue;
            }
            
            printf(socketstate.c_str());
     
            if (_stricmp(g_battlenetname, filename) ==0)
            {
                g_battlenet.push_back(pTcpTable->table[i]);
            }
            if (_stricmp(g_Hearthstonename, filename) == 0)
            {
                g_Hearthstone.push_back(pTcpTable->table[i]);
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

    do
    {

        //SetTcpEntry((MIB_TCPROW*)&srtTcpRow);
       // pTcpTable->table[i].dwState = MIB_TCP_STATE_DELETE_TCB;
        //    dwRetVal = SetTcpEntry((MIB_TCPROW*)&pTcpTable->table[i]);
        if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_DELETE)&1)
        {
            printf("正在拔线....\n");
            get_list(&g_battlenet, &g_Hearthstone);
            bool samesocket = false;
         
            for (int i = 0; i < g_Hearthstone.size(); i++)
            {
                samesocket = false;
                for (size_t k = 0; k < g_battlenet.size(); k++)
                {
                    if (g_Hearthstone[i].dwRemoteAddr== g_battlenet[k].dwRemoteAddr && g_Hearthstone[i].dwRemotePort == g_battlenet[k].dwRemotePort)
                    {
                        samesocket = true;
                        break;
                    }
                }
                if (samesocket==false)
                {
                    g_Hearthstone[i].dwState = MIB_TCP_STATE_DELETE_TCB;//重置网络连接
                    
                    char szLocalAddr[128];
                    char szRemoteAddr[128];
                    InetNtopA(AF_INET, &g_Hearthstone[i].dwLocalAddr, szLocalAddr, 128);
                    DWORD dwLocalPort = ntohs(g_Hearthstone[i].dwLocalPort);
               
                    InetNtopA(AF_INET, &g_Hearthstone[i].dwRemoteAddr, szRemoteAddr, 128);
                    DWORD dwRemotePort = ntohs(g_Hearthstone[i].dwRemotePort);
                    printf("已经断开连接--->%s:%d ---- %s:%d \n", szLocalAddr, dwLocalPort, szRemoteAddr, dwRemotePort);
                    SetTcpEntry((MIB_TCPROW*)&g_Hearthstone[i]);
                }
            }
            Sleep(500);
        }
       
        
        
        Sleep(10);
    } while (true);
    
    
    return 0;
}
int main()
{
   
    CreateThread(0, 0, thread_resocket, 0, 0, 0);
    printf("启动成功\n登录[暴雪战网]且[炉石传说]游戏中\n按下热键[Ctrl+Delete]拔线");
    getchar();
    return 0;
}