// Trabalho de Sistemas Operacionais - FMS (Gerenciador de Processos Simples)
// Alunos: João Vitor dos Santos Pereira

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <psapi.h>

// estrutura global para acessar o processo na thread
PROCESS_INFORMATION pi;

// função pra converter FILETIME (formato estranho do Windows)
// pra um número que a gente consegue somar
ULONGLONG filetimeToULL(FILETIME ft) {
    return (((ULONGLONG)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// thread que fica monitorando o processo
DWORD WINAPI monitor(LPVOID arg) {

    // limites passados pelo main
    int limite_tempo = ((int*)arg)[0];
    int limite_cpu = ((int*)arg)[1];
    int limite_mem = ((int*)arg)[2];

    int tempo = 0;

    while (1) {
        Sleep(1000); // espera 1 segundo
        tempo++;

        // verifica se o processo ainda está rodando
        DWORD status;
        GetExitCodeProcess(pi.hProcess, &status);
        if (status != STILL_ACTIVE) break;

        // pega tempo de CPU
        FILETIME creation, exit, kernel, user;
        GetProcessTimes(pi.hProcess, &creation, &exit, &kernel, &user);

        ULONGLONG cpu = filetimeToULL(kernel) + filetimeToULL(user);
        double cpu_seg = cpu / 10000000.0; // converter pra segundos

        // pega uso de memória
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc));

        double memoria_mb = pmc.WorkingSetSize / (1024.0 * 1024.0);

        // mostra tudo na tela
        printf("Tempo: %d s | CPU: %.2f s | Memoria: %.2f MB\n",
               tempo, cpu_seg, memoria_mb);

        // verifica se passou do tempo
        if (tempo >= limite_tempo) {
            printf("Timeout atingido!\n");
            TerminateProcess(pi.hProcess, 0);
            break;
        }

        // verifica CPU
        if (cpu_seg >= limite_cpu) {
            printf("Limite de CPU atingido!\n");
            TerminateProcess(pi.hProcess, 0);
            break;
        }

        // verifica memória
        if (memoria_mb >= limite_mem) {
            printf("Limite de memoria atingido!\n");
            TerminateProcess(pi.hProcess, 0);
            break;
        }
    }

    return 0;
}

int main() {

    double cpu_total = 0;      // CPU acumulada
    double limite_global;      // limite total do FMS

    printf("Quota total de CPU do FMS (segundos): ");
    scanf("%lf", &limite_global);

    // loop principal (FMS fica rodando até acabar a quota)
    while (1) {

        // se passou do limite global, encerra tudo
        if (cpu_total >= limite_global) {
            printf("\nQuota global de CPU atingida. Encerrando FMS...\n");
            break;
        }

        char programa[200];
        int limites[3]; // tempo, cpu, memoria

        printf("\nPrograma (ou sair): ");
        scanf(" %[^\n]", programa);

        // opção de sair manual
        if (strcmp(programa, "sair") == 0) break;

        // leitura dos limites
        printf("Timeout (segundos): ");
        scanf("%d", &limites[0]);

        printf("Limite de CPU (segundos): ");
        scanf("%d", &limites[1]);

        printf("Limite de memoria (MB): ");
        scanf("%d", &limites[2]);

        STARTUPINFO si;

        // limpa estruturas
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // cria o processo
        if (!CreateProcess(NULL, programa, NULL, NULL, FALSE,
                           CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            printf("Erro ao criar processo. Codigo: %lu\n", GetLastError());
            continue;
        }

        printf("Processo iniciado!\n");

        // cria thread pra monitorar
        HANDLE thread = CreateThread(NULL, 0, monitor, limites, 0, NULL);

        // espera o processo terminar
        WaitForSingleObject(pi.hProcess, INFINITE);

        // pega CPU final usada
        FILETIME creation, exit, kernel, user;
        GetProcessTimes(pi.hProcess, &creation, &exit, &kernel, &user);

        ULONGLONG cpu = filetimeToULL(kernel) + filetimeToULL(user);
        double cpu_seg = cpu / 10000000.0;

        // soma no total
        cpu_total += cpu_seg;

        printf("Processo finalizado.\n");
        printf("CPU usada nesta execucao: %.2f s\n", cpu_seg);
        printf("CPU acumulada: %.2f / %.2f s\n", cpu_total, limite_global);

        // encerra thread e libera memória
        TerminateThread(thread, 0);

        CloseHandle(thread);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return 0;
}