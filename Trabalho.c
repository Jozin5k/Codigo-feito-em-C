// trabalho de SO - FMS simples
// aluno: João Vitor dos Santos Pereira

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <psapi.h>

// variavel global pra thread conseguir acessar o processo
// (nao achei jeito melhor de fazer isso)
PROCESS_INFORMATION pi;

// converte aquele negocio esquisito de tempo do windows pra um numero normal
ULONGLONG convertetempo(FILETIME ft) {
    ULONGLONG resultado = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return resultado;
}

// essa thread roda em paralelo e fica de olho no processo
DWORD WINAPI threadMonitor(LPVOID arg) {

    int *limites = (int*)arg;
    int tmax  = limites[0];
    int cpumax = limites[1];
    int memmax = limites[2];

    int segundos = 0;

    while (1) {
        Sleep(1000);
        segundos++;

        // checa se o processo ainda ta rodando
        DWORD saida;
        GetExitCodeProcess(pi.hProcess, &saida);
        if (saida != STILL_ACTIVE) {
            break;
        }

        // pega os tempos de CPU do processo
        FILETIME tcriacao, tfim, tkernel, tusuario;
        GetProcessTimes(pi.hProcess, &tcriacao, &tfim, &tkernel, &tusuario);

        // soma kernel + usuario e converte pra segundos
        // o windows usa unidades de 100 nanossegundos entao divide por 10000000
        double cpu_seg = (convertetempo(tkernel) + convertetempo(tusuario)) / 10000000.0;

        // pega o uso de memoria
        PROCESS_MEMORY_COUNTERS info_mem;
        GetProcessMemoryInfo(pi.hProcess, &info_mem, sizeof(info_mem));
        double mem_mb = info_mem.WorkingSetSize / (1024.0 * 1024.0);

        printf("tempo: %ds | cpu usada: %.2fs | memoria: %.2fMB\n", segundos, cpu_seg, mem_mb);

        // verifica os limites um por um
        if (segundos >= tmax) {
            printf(">> timeout atingido, matando processo...\n");
            TerminateProcess(pi.hProcess, 0);
            break;
        }

        if (cpu_seg >= cpumax) {
            printf(">> limite de cpu atingido, matando processo...\n");
            TerminateProcess(pi.hProcess, 0);
            break;
        }

        if (mem_mb >= memmax) {
            printf(">> limite de memoria atingido, matando processo...\n");
            TerminateProcess(pi.hProcess, 0);
            break;
        }
    }

    return 0;
}

int main() {

    double cpu_acumulada = 0;
    double cota_global;

    printf("informe a cota total de CPU do FMS (em segundos): ");
    scanf("%lf", &cota_global);

    // loop principal, o FMS fica rodando ate acabar a cota
    while (1) {

        // verifica se ainda tem cota disponivel
        if (cpu_acumulada >= cota_global) {
            printf("\ncota de CPU esgotada. encerrando FMS...\n");
            break;
        }

        char programa[200];
        int limites[3];

        printf("\ndigite o programa pra executar (ou 'sair'): ");
        scanf(" %[^\n]", programa);

        if (strcmp(programa, "sair") == 0) {
            break;
        }

        printf("timeout em segundos: ");
        scanf("%d", &limites[0]);

        printf("limite de CPU em segundos: ");
        scanf("%d", &limites[1]);

        printf("limite de memoria em MB: ");
        scanf("%d", &limites[2]);

        // prepara as estruturas antes de criar o processo
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // tenta criar o processo
        if (!CreateProcess(NULL, programa, NULL, NULL, FALSE,
                           CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            printf("nao consegui criar o processo. codigo de erro: %lu\n", GetLastError());
            continue;
        }

        printf("processo criado com sucesso!\n");

        // cria a thread de monitoramento passando os limites
        HANDLE hthread = CreateThread(NULL, 0, threadMonitor, limites, 0, NULL);

        // espera o processo terminar (seja pelo timeout ou normalmente)
        WaitForSingleObject(pi.hProcess, INFINITE);

        // depois que terminou, pega o cpu total que ele usou
        FILETIME tcriacao, tfim, tkernel, tusuario;
        GetProcessTimes(pi.hProcess, &tcriacao, &tfim, &tkernel, &tusuario);

        double cpu_usada = (converteempo(tkernel) + converteempo(tusuario)) / 10000000.0;

        cpu_acumulada += cpu_usada;

        printf("\nprocesso finalizado.\n");
        printf("cpu usada agora: %.2fs\n", cpu_usada);
        printf("cpu acumulada ate agora: %.2fs / %.2fs\n", cpu_acumulada, cota_global);

        // fecha tudo certinho
        TerminateThread(hthread, 0);
        CloseHandle(hthread);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return 0;
}
