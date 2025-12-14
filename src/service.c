#include "../include/service.h"
#include "../include/bolt_server.h"
#include "../include/config.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVICE_NAME_MAX 256

static SERVICE_STATUS g_service_status;
static SERVICE_STATUS_HANDLE g_service_status_handle = NULL;
static BoltServer* g_service_server = NULL;

/*
 * Service control handler.
 */
static void WINAPI service_ctrl_handler(DWORD ctrl_code) {
    switch (ctrl_code) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            g_service_status.dwWin32ExitCode = 0;
            g_service_status.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_service_status_handle, &g_service_status);
            
            if (g_service_server) {
                bolt_server_stop(g_service_server);
            }
            break;
        default:
            break;
    }
}

/*
 * Service main function.
 */
static void WINAPI service_main(DWORD argc, LPTSTR* argv) {
    (void)argc;
    (void)argv;
    
    g_service_status_handle = RegisterServiceCtrlHandlerA("BoltServer", service_ctrl_handler);
    if (!g_service_status_handle) {
        return;
    }
    
    g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_service_status.dwCurrentState = SERVICE_START_PENDING;
    g_service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_service_status.dwWin32ExitCode = 0;
    g_service_status.dwServiceSpecificExitCode = 0;
    g_service_status.dwCheckPoint = 0;
    g_service_status.dwWaitHint = 0;
    
    SetServiceStatus(g_service_status_handle, &g_service_status);
    
    /* Load config and start server */
    BoltConfig config;
    config_load_defaults(&config);
    
    /* TODO: Load config from file */
    g_service_server = bolt_server_create_with_config(&config);
    if (g_service_server) {
        g_service_status.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus(g_service_status_handle, &g_service_status);
        
        bolt_server_run(g_service_server);
        
        bolt_server_destroy(g_service_server);
        g_service_server = NULL;
    }
    
    g_service_status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_service_status_handle, &g_service_status);
}

/*
 * Install Bolt as a Windows Service.
 */
bool service_install(const char* service_name, const char* display_name,
                     const char* description, const char* config_path) {
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        return false;
    }
    
    SC_HANDLE sc_manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!sc_manager) {
        return false;
    }
    
    char service_cmd[1024];
    if (config_path && config_path[0]) {
        snprintf(service_cmd, sizeof(service_cmd), "\"%s\" -c \"%s\"", exe_path, config_path);
    } else {
        snprintf(service_cmd, sizeof(service_cmd), "\"%s\"", exe_path);
    }
    
    SC_HANDLE service = CreateServiceA(
        sc_manager,
        service_name,
        display_name,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        service_cmd,
        NULL, NULL, NULL, NULL, NULL
    );
    
    bool success = (service != NULL);
    
    if (service) {
        /* Set description */
        if (description) {
            SERVICE_DESCRIPTION sd;
            sd.lpDescription = (LPSTR)description;
            ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &sd);
        }
        CloseServiceHandle(service);
    }
    
    CloseServiceHandle(sc_manager);
    return success;
}

/*
 * Uninstall Bolt Windows Service.
 */
bool service_uninstall(const char* service_name) {
    SC_HANDLE sc_manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!sc_manager) {
        return false;
    }
    
    SC_HANDLE service = OpenServiceA(sc_manager, service_name, DELETE);
    if (!service) {
        CloseServiceHandle(sc_manager);
        return false;
    }
    
    bool success = DeleteService(service) != 0;
    
    CloseServiceHandle(service);
    CloseServiceHandle(sc_manager);
    
    return success;
}

/*
 * Run Bolt as a Windows Service.
 */
bool service_run(const char* service_name, int argc, char* argv[]) {
    (void)service_name;
    (void)argc;
    (void)argv;
    
    SERVICE_TABLE_ENTRYA service_table[] = {
        { "BoltServer", service_main },
        { NULL, NULL }
    };
    
    return StartServiceCtrlDispatcherA(service_table) != 0;
}

/*
 * Check if running as a service.
 */
bool service_is_running(void) {
    return (g_service_status_handle != NULL);
}

