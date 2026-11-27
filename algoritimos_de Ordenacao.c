#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define _CRT_SECURE_NO_WARNINGS
  #define __USE_MINGW_ANSI_STDIO 1
#elif defined(__linux__)
  #define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
  #include <cpuid.h>
  #define CPUID_GCC 1
#elif defined(_M_X64) || defined(_M_IX86)
  #include <intrin.h>
  #define CPUID_MSVC 1
#endif

// Configuracoes dos testes
#define SEED_PADRAO          UINT64_C(20261127)
#define REPETICOES_MINIMAS   3
#define REPETICOES_MAXIMAS   100
#define VALOR_MAXIMO         1000000
#define N_LIMITE             10000000
#define MAX_PONTOS           20
#define ELEMENTOS_CROSSOVER  100000
#define N_TESTE_K            100000

static void trocar(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

// Aloca e preenche a memoria antes de medir o tempo
static void *alocar(size_t quantidade, size_t tamanho)
{
    size_t q = quantidade ? quantidade : 1;
    void *p = NULL;
    if (q <= SIZE_MAX / tamanho)
        p = malloc(q * tamanho);
    if (p == NULL) {
        fprintf(stderr, "\nERRO: memoria insuficiente (%" PRIu64 " x %" PRIu64 " bytes).\n",
                (uint64_t)q, (uint64_t)tamanho);
        exit(EXIT_FAILURE);
    }
    memset(p, 0xA5, q * tamanho);
    return p;
}

// Marca o tempo em segundos
static double agora_segundos(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequencia, contador;
    QueryPerformanceFrequency(&frequencia);
    QueryPerformanceCounter(&contador);
    return (double)contador.QuadPart / (double)frequencia.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
#endif
}

// Gerador proprio: a mesma seed gera os mesmos numeros
static uint64_t proximo_aleatorio(uint64_t *estado)
{
    uint64_t z = (*estado += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

// Sorteia um numero de 0 ate o maximo
static int sortear(uint64_t *estado, int maximo)
{
    uint64_t faixa = (uint64_t)maximo + 1;
    uint64_t limite = (0 - faixa) % faixa;
    uint64_t r;
    do {
        r = proximo_aleatorio(estado);
    } while (r < limite);
    return (int)(r % faixa);
}

// Gera o vetor a partir da seed, do tamanho e do valor maximo
static void gerar_dados(int *v, size_t total, uint64_t seed, size_t n, int maximo)
{
    uint64_t estado = seed;
    estado = proximo_aleatorio(&estado) ^ (uint64_t)n;
    estado = proximo_aleatorio(&estado) ^ (uint64_t)maximo;
    for (size_t i = 0; i < total; i++)
        v[i] = sortear(&estado, maximo);
}

// Codigo para conferir se os vetores sao iguais
static uint64_t calcular_hash(const int *v, size_t n)
{
    uint64_t h = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < n; i++) {
        uint32_t x = (uint32_t)v[i];
        for (int b = 0; b < 4; b++) {
            h ^= (x >> (8 * b)) & 0xFFu;
            h *= UINT64_C(1099511628211);
        }
    }
    return h;
}

// Vetores auxiliares, criados antes da medicao
typedef struct {
    int      *temporario;
    uint32_t *contadores;
    size_t    capacidade_contadores;
    uint32_t *baldes;
} AreaTrabalho;

static AreaTrabalho criar_area(size_t n, size_t amplitude)
{
    AreaTrabalho area;
    area.temporario = alocar(n, sizeof(int));
    area.contadores = alocar(amplitude, sizeof(uint32_t));
    area.capacidade_contadores = amplitude;
    area.baldes = alocar(n, sizeof(uint32_t));
    return area;
}

static void liberar_area(AreaTrabalho *area)
{
    free(area->temporario);
    free(area->contadores);
    free(area->baldes);
}

static void obter_min_max(const int *v, size_t n, int *minimo, int *maximo)
{
    int mn = v[0], mx = v[0];
    for (size_t i = 1; i < n; i++) {
        if (v[i] < mn) mn = v[i];
        if (v[i] > mx) mx = v[i];
    }
    *minimo = mn;
    *maximo = mx;
}

// Bubble Sort: troca vizinhos que estao fora de ordem
static void bubble_sort(int *v, size_t n, AreaTrabalho *area)
{
    (void)area;
    for (size_t fim = n; fim > 1; fim--) {
        int trocou = 0;
        for (size_t i = 1; i < fim; i++) {
            if (v[i - 1] > v[i]) {
                trocar(&v[i - 1], &v[i]);
                trocou = 1;
            }
        }
        if (!trocou)
            break;
    }
}

// Selection Sort: leva o menor elemento para o inicio
static void selection_sort(int *v, size_t n, AreaTrabalho *area)
{
    (void)area;
    for (size_t i = 0; i + 1 < n; i++) {
        size_t menor = i;
        for (size_t j = i + 1; j < n; j++)
            if (v[j] < v[menor])
                menor = j;
        if (menor != i)
            trocar(&v[i], &v[menor]);
    }
}

// Insertion Sort: coloca cada elemento na posicao certa
static void inserir_ordenado(int *v, size_t n)
{
    for (size_t i = 1; i < n; i++) {
        int chave = v[i];
        size_t j = i;
        while (j > 0 && v[j - 1] > chave) {
            v[j] = v[j - 1];
            j--;
        }
        v[j] = chave;
    }
}

static void insertion_sort(int *v, size_t n, AreaTrabalho *area)
{
    (void)area;
    inserir_ordenado(v, n);
}

// Merge Sort: divide o vetor ao meio e junta as partes ordenadas
static void merge_recursivo(int *v, int *tmp, size_t inicio, size_t fim)
{
    if (fim - inicio < 2)
        return;
    size_t meio = inicio + (fim - inicio) / 2;
    merge_recursivo(v, tmp, inicio, meio);
    merge_recursivo(v, tmp, meio, fim);

    size_t i = inicio, j = meio, k = inicio;
    while (i < meio && j < fim)
        tmp[k++] = (v[j] < v[i]) ? v[j++] : v[i++];
    while (i < meio)
        tmp[k++] = v[i++];
    while (j < fim)
        tmp[k++] = v[j++];
    memcpy(v + inicio, tmp + inicio, (fim - inicio) * sizeof(int));
}

static void merge_sort(int *v, size_t n, AreaTrabalho *area)
{
    merge_recursivo(v, area->temporario, 0, n);
}

// Quick Sort: separa os valores usando um pivo
static void quick_recursivo(int *v, ptrdiff_t baixo, ptrdiff_t alto)
{
    while (baixo < alto) {
        ptrdiff_t meio = baixo + (alto - baixo) / 2;

        // Escolhe o pivo pela mediana de tres
        if (v[meio] < v[baixo]) trocar(&v[meio], &v[baixo]);
        if (v[alto] < v[baixo]) trocar(&v[alto], &v[baixo]);
        if (v[alto] < v[meio])  trocar(&v[alto], &v[meio]);

        int pivo = v[meio];
        ptrdiff_t i = baixo - 1, j = alto + 1;
        for (;;) {
            do { i++; } while (v[i] < pivo);
            do { j--; } while (v[j] > pivo);
            if (i >= j)
                break;
            trocar(&v[i], &v[j]);
        }

        // Faz recursao na parte menor para usar menos pilha
        if (j - baixo < alto - j - 1) {
            quick_recursivo(v, baixo, j);
            baixo = j + 1;
        } else {
            quick_recursivo(v, j + 1, alto);
            alto = j;
        }
    }
}

static void quick_sort(int *v, size_t n, AreaTrabalho *area)
{
    (void)area;
    if (n > 1)
        quick_recursivo(v, 0, (ptrdiff_t)n - 1);
}

// Heap Sort: ajusta o heap para o maior ficar no topo
static void descer_no_heap(int *v, size_t raiz, size_t tamanho)
{
    int valor = v[raiz];
    for (;;) {
        size_t filho = 2 * raiz + 1;
        if (filho >= tamanho)
            break;
        if (filho + 1 < tamanho && v[filho + 1] > v[filho])
            filho++;
        if (v[filho] <= valor)
            break;
        v[raiz] = v[filho];
        raiz = filho;
    }
    v[raiz] = valor;
}

static void heap_sort(int *v, size_t n, AreaTrabalho *area)
{
    (void)area;
    for (size_t i = n / 2; i > 0; i--)
        descer_no_heap(v, i - 1, n);
    for (size_t fim = n; fim > 1; fim--) {
        trocar(&v[0], &v[fim - 1]);
        descer_no_heap(v, 0, fim - 1);
    }
}

// Counting Sort: conta quantas vezes cada valor aparece
static void counting_sort(int *v, size_t n, AreaTrabalho *area)
{
    if (n < 2)
        return;
    int minimo, maximo;
    obter_min_max(v, n, &minimo, &maximo);
    size_t k = (size_t)((int64_t)maximo - minimo) + 1;
    if (k > area->capacidade_contadores) {
        fprintf(stderr, "\nERRO: Counting Sort sem espaco para K = %" PRIu64 ".\n", (uint64_t)k);
        exit(EXIT_FAILURE);
    }
    uint32_t *c = area->contadores;
    memset(c, 0, k * sizeof(uint32_t));

    for (size_t i = 0; i < n; i++)
        c[(int64_t)v[i] - minimo]++;
    for (size_t i = 1; i < k; i++)
        c[i] += c[i - 1];
    for (size_t i = n; i > 0; i--) {
        int x = v[i - 1];
        area->temporario[--c[(int64_t)x - minimo]] = x;
    }
    memcpy(v, area->temporario, n * sizeof(int));
}

// Radix Sort: ordena um byte por vez, do menos significativo
static void radix_sort(int *v, size_t n, AreaTrabalho *area)
{
    if (n < 2)
        return;
    int minimo, maximo;
    obter_min_max(v, n, &minimo, &maximo);
    uint32_t maior_chave = (uint32_t)((int64_t)maximo - minimo);

    int *origem = v;
    int *destino = area->temporario;

    for (int desloc = 0; desloc < 32 && (maior_chave >> desloc) != 0; desloc += 8) {
        uint32_t c[256] = {0};
        for (size_t i = 0; i < n; i++)
            c[((uint32_t)((int64_t)origem[i] - minimo) >> desloc) & 0xFFu]++;
        for (int d = 1; d < 256; d++)
            c[d] += c[d - 1];
        for (size_t i = n; i > 0; i--) {
            int x = origem[i - 1];
            uint32_t d = ((uint32_t)((int64_t)x - minimo) >> desloc) & 0xFFu;
            destino[--c[d]] = x;
        }
        int *t = origem;
        origem = destino;
        destino = t;
    }
    if (origem != v)
        memcpy(v, origem, n * sizeof(int));
}

// Bucket Sort: separa em baldes e usa Insertion Sort em cada um
static size_t indice_balde(int x, int minimo, uint64_t amplitude, size_t n)
{
    return (size_t)((uint64_t)((int64_t)x - minimo) * (uint64_t)n / amplitude);
}

static void bucket_sort(int *v, size_t n, AreaTrabalho *area)
{
    if (n < 2)
        return;
    int minimo, maximo;
    obter_min_max(v, n, &minimo, &maximo);
    if (minimo == maximo)
        return;
    uint64_t amplitude = (uint64_t)((int64_t)maximo - minimo) + 1;
    uint32_t *fim = area->baldes;
    int *tmp = area->temporario;
    memset(fim, 0, n * sizeof(uint32_t));

    for (size_t i = 0; i < n; i++)
        fim[indice_balde(v[i], minimo, amplitude, n)]++;
    for (size_t b = 1; b < n; b++)
        fim[b] += fim[b - 1];
    for (size_t i = n; i > 0; i--) {
        int x = v[i - 1];
        tmp[--fim[indice_balde(x, minimo, amplitude, n)]] = x;
    }
    for (size_t b = 0; b < n; b++) {
        size_t ini = fim[b];
        size_t prox = (b + 1 < n) ? fim[b + 1] : n;
        if (prox - ini > 1)
            inserir_ordenado(tmp + ini, prox - ini);
    }
    memcpy(v, tmp, n * sizeof(int));
}

typedef void (*FuncaoOrdenacao)(int *, size_t, AreaTrabalho *);

typedef struct {
    const char *nome;
    const char *categoria;
    const char *complexidade_media;
    FuncaoOrdenacao ordenar;
} Algoritmo;

#define TOTAL_ALGORITMOS 9
#define PRIMEIRO_EFICIENTE 3
enum { IDX_INSERTION = 2, IDX_MERGE = 3, IDX_QUICK = 4, IDX_HEAP = 5 };

// Lista com os 9 algoritmos
static const Algoritmo ALGORITMOS[TOTAL_ALGORITMOS] = {
    {"Bubble Sort",    "Comparacao",     "O(n^2)",     bubble_sort},
    {"Selection Sort", "Comparacao",     "O(n^2)",     selection_sort},
    {"Insertion Sort", "Comparacao",     "O(n^2)",     insertion_sort},
    {"Merge Sort",     "Comparacao",     "O(n log n)", merge_sort},
    {"Quick Sort",     "Comparacao",     "O(n log n)", quick_sort},
    {"Heap Sort",      "Comparacao",     "O(n log n)", heap_sort},
    {"Counting Sort",  "Sem comparacao", "O(n + K)",   counting_sort},
    {"Radix Sort",     "Sem comparacao", "O(d(n + b))", radix_sort},
    {"Bucket Sort",    "Sem comparacao", "O(n) esperado (dados uniformes)", bucket_sort}
};

// Memoria auxiliar calculada em bytes (sem contar a pilha)
static uint64_t memoria_auxiliar(int indice, size_t n, size_t amplitude)
{
    uint64_t vetor = (uint64_t)n * sizeof(int);
    switch (indice) {
        case 3:  return vetor;
        case 6:  return vetor + (uint64_t)amplitude * sizeof(uint32_t);
        case 7:  return vetor;
        case 8:  return vetor + (uint64_t)n * sizeof(uint32_t);
        default: return 0;
    }
}

static int comparar_inteiros(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

// Testa os algoritmos e compara o resultado com o qsort
static int executar_autoteste(void)
{
    static const size_t tamanhos[] = {0, 1, 2, 3, 4, 5, 10, 17, 100, 255, 256, 257, 1000};
    const int total_tamanhos = (int)(sizeof(tamanhos) / sizeof(tamanhos[0]));
    enum { PADROES = 8, MAIOR = 1000, AMPLITUDE = 200001 };

    int original[MAIOR], esperado[MAIOR], obtido[MAIOR];
    AreaTrabalho area = criar_area(MAIOR, AMPLITUDE);
    uint64_t estado = 12345;
    int casos = 0, falhas = 0;

    for (int t = 0; t < total_tamanhos; t++) {
        size_t n = tamanhos[t];
        for (int p = 0; p < PADROES; p++) {
            for (size_t i = 0; i < n; i++) {
                switch (p) {
                    case 0:  original[i] = sortear(&estado, 100000); break;
                    case 1:  original[i] = (int)i; break;
                    case 2:  original[i] = (int)(n - i); break;
                    case 3:  original[i] = 7; break;
                    case 4:  original[i] = sortear(&estado, 3); break;
                    case 5:  original[i] = sortear(&estado, 200000) - 100000; break;
                    case 6:  original[i] = (i == 0) ? 100000 : 0; break;
                    default: original[i] = (int)(i % 2 ? i : n - i); break;
                }
            }
            if (n > 0)
                memcpy(esperado, original, n * sizeof(int));
            qsort(esperado, n, sizeof(int), comparar_inteiros);

            for (int a = 0; a < TOTAL_ALGORITMOS; a++) {
                memcpy(obtido, original, n * sizeof(int));
                ALGORITMOS[a].ordenar(obtido, n, &area);
                casos++;
                if (memcmp(obtido, esperado, n * sizeof(int)) != 0) {
                    printf("  FALHA: %s (n=%d, padrao=%d)\n", ALGORITMOS[a].nome, (int)n, p);
                    falhas++;
                }
            }
        }
    }
    liberar_area(&area);
    printf("Autoteste de correcao: %d casos, %d falha(s).\n", casos, falhas);
    return falhas == 0;
}

static void obter_processador(char *saida, size_t tamanho)
{
    snprintf(saida, tamanho, "nao identificado (preencher manualmente)");
#if defined(CPUID_GCC) || defined(CPUID_MSVC)
    unsigned int r[12] = {0};
    char marca[49];
  #ifdef CPUID_GCC
    if (__get_cpuid_max(0x80000000u, NULL) < 0x80000004u)
        return;
    for (unsigned int i = 0; i < 3; i++)
        __get_cpuid(0x80000002u + i, &r[4 * i], &r[4 * i + 1], &r[4 * i + 2], &r[4 * i + 3]);
  #else
    int info[4];
    __cpuid(info, (int)0x80000000u);
    if ((unsigned int)info[0] < 0x80000004u)
        return;
    for (int i = 0; i < 3; i++)
        __cpuid((int *)&r[4 * i], (int)(0x80000002u + (unsigned int)i));
  #endif
    memcpy(marca, r, 48);
    marca[48] = '\0';
    const char *inicio = marca;
    while (*inicio == ' ')
        inicio++;
    if (*inicio)
        snprintf(saida, tamanho, "%s", inicio);
#endif
}

static double obter_ram_gib(void)
{
#ifdef _WIN32
    MEMORYSTATUSEX m;
    m.dwLength = sizeof(m);
    if (GlobalMemoryStatusEx(&m))
        return (double)m.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    long paginas = sysconf(_SC_PHYS_PAGES), tam = sysconf(_SC_PAGESIZE);
    if (paginas > 0 && tam > 0)
        return (double)paginas * (double)tam / (1024.0 * 1024.0 * 1024.0);
#endif
    return -1.0;
}

// Mostra as informacoes do computador e do compilador
static void escrever_ambiente(FILE *f, uint64_t seed, int repeticoes)
{
    char processador[128];
    obter_processador(processador, sizeof(processador));
    double ram = obter_ram_gib();

    fprintf(f, "Processador : %s\n", processador);
    if (ram > 0)
        fprintf(f, "RAM visivel : %.1f GiB\n", ram);
    else
        fprintf(f, "RAM visivel : nao identificada (preencher manualmente)\n");
#if defined(_WIN32)
    fprintf(f, "Sistema     : Windows (anotar a versao exata)\n");
#elif defined(__APPLE__)
    fprintf(f, "Sistema     : macOS (anotar a versao exata)\n");
#elif defined(__linux__)
    fprintf(f, "Sistema     : Linux (anotar a distribuicao e versao)\n");
#else
    fprintf(f, "Sistema     : outro (preencher manualmente)\n");
#endif
#if defined(__clang__)
    fprintf(f, "Linguagem   : C (padrao %ldL), compilador Clang %s\n", (long)__STDC_VERSION__, __clang_version__);
#elif defined(__GNUC__)
    fprintf(f, "Linguagem   : C (padrao %ldL), compilador GCC %s\n", (long)__STDC_VERSION__, __VERSION__);
#elif defined(_MSC_VER)
    fprintf(f, "Linguagem   : C, compilador MSVC %d\n", _MSC_VER);
#else
    fprintf(f, "Linguagem   : C\n");
#endif
#if defined(__OPTIMIZE__)
    fprintf(f, "Otimizacao  : ativada (anotar a flag usada, ex.: -O2)\n");
#elif defined(__GNUC__)
    fprintf(f, "Otimizacao  : DESATIVADA (recompile com -O2)\n");
#else
    fprintf(f, "Otimizacao  : anotar a flag usada (ex.: /O2)\n");
#endif
    fprintf(f, "Seed        : %" PRIu64 "\n", seed);
    fprintf(f, "Repeticoes  : %d\n", repeticoes);
}

static char carimbo[40];

// Cria os arquivos de resultado
static FILE *abrir_saida(const char *tipo, const char *experimento, const char *extensao)
{
    char nome[160];
    snprintf(nome, sizeof(nome), "%s_%s_%s.%s", tipo, experimento, carimbo, extensao);
    FILE *f = fopen(nome, "w");
    if (f == NULL) {
        fprintf(stderr, "ERRO: nao foi possivel criar o arquivo %s (%s).\n", nome, strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("Arquivo: %s\n", nome);
    return f;
}

static void escrever_decimal(FILE *f, double valor)
{
    char texto[64];
    snprintf(texto, sizeof(texto), "%.6f", valor);
    char *ponto = strchr(texto, '.');
    if (ponto)
        *ponto = ',';
    fputs(texto, f);
}

typedef struct {
    size_t n;
    int    valor_maximo;
    size_t vetores;
    double media_ms[TOTAL_ALGORITMOS];
} Ponto;

// Executa e mede todos os algoritmos para um tamanho N
static void medir_ponto(Ponto *p, uint64_t seed, int repeticoes, int primeiro_algoritmo,
                        FILE *execucoes, FILE *resumo)
{
    size_t n = p->n, m = p->vetores, total = n * m;
    size_t amplitude = (size_t)p->valor_maximo + 1;

    int *base       = alocar(total, sizeof(int));
    int *referencia = alocar(total, sizeof(int));
    int *trabalho   = alocar(total, sizeof(int));
    AreaTrabalho area = criar_area(n, amplitude);

    gerar_dados(base, total, seed, n, p->valor_maximo);
    uint64_t hash = calcular_hash(base, total);

    memcpy(referencia, base, total * sizeof(int));
    for (size_t i = 0; i < m; i++)
        qsort(referencia + i * n, n, sizeof(int), comparar_inteiros);

    printf("\n--- N = %d | valores de 0 a %d | vetores no lote: %d | hash: %016" PRIX64 " ---\n",
           (int)n, p->valor_maximo, (int)m, hash);

    for (int a = 0; a < TOTAL_ALGORITMOS; a++) {
        p->media_ms[a] = -1.0;
        if (a < primeiro_algoritmo)
            continue;

        double soma = 0, menor = 0, maior = 0;
        printf("  %-15s", ALGORITMOS[a].nome);
        fflush(stdout);

        for (int r = 0; r < repeticoes; r++) {
            // Cada execucao recebe uma copia do mesmo vetor
            memcpy(trabalho, base, total * sizeof(int));

            // O tempo medido e o da ordenacao
            double inicio = agora_segundos();
            for (size_t i = 0; i < m; i++)
                ALGORITMOS[a].ordenar(trabalho + i * n, n, &area);
            double final = agora_segundos();

            double ms = (final - inicio) * 1000.0 / (double)m;

            // Confere o resultado fora do tempo medido
            if (memcmp(trabalho, referencia, total * sizeof(int)) != 0) {
                fprintf(stderr, "\nERRO: %s ordenou incorretamente (N=%d).\n",
                        ALGORITMOS[a].nome, (int)n);
                exit(EXIT_FAILURE);
            }

            soma += ms;
            if (r == 0 || ms < menor) menor = ms;
            if (r == 0 || ms > maior) maior = ms;

            fprintf(execucoes, "%" PRIu64 ";%d;%d;%d;%016" PRIX64 ";%s;%d;",
                    seed, (int)n, p->valor_maximo, (int)m, hash, ALGORITMOS[a].nome, r + 1);
            escrever_decimal(execucoes, ms);
            fputc('\n', execucoes);
            fflush(execucoes);
            printf(".");
            fflush(stdout);
        }

        p->media_ms[a] = soma / repeticoes;
        printf(" media: %14.6f ms  [OK]\n", p->media_ms[a]);

        fprintf(resumo, "%" PRIu64 ";%d;%d;%d;%016" PRIX64 ";%s;%s;%s;%d;",
                seed, (int)n, p->valor_maximo, (int)m, hash, ALGORITMOS[a].categoria,
                ALGORITMOS[a].nome, ALGORITMOS[a].complexidade_media, repeticoes);
        escrever_decimal(resumo, p->media_ms[a]);
        fputc(';', resumo);
        escrever_decimal(resumo, menor);
        fputc(';', resumo);
        escrever_decimal(resumo, maior);
        fprintf(resumo, ";%" PRIu64 "\n", memoria_auxiliar(a, n, amplitude));
        fflush(resumo);
    }

    free(base);
    free(referencia);
    free(trabalho);
    liberar_area(&area);
}

static void salvar_tabela(const char *experimento, const Ponto *pontos, int quantidade,
                          int primeiro_algoritmo, int coluna_por_k)
{
    FILE *f = abrir_saida("tabela", experimento, "csv");
    fprintf(f, "Categoria;Algoritmo;Complexidade media");
    for (int i = 0; i < quantidade; i++) {
        if (coluna_por_k)
            fprintf(f, ";valores 0 a %d (ms)", pontos[i].valor_maximo);
        else
            fprintf(f, ";N = %d (ms)", (int)pontos[i].n);
    }
    fputc('\n', f);
    for (int a = primeiro_algoritmo; a < TOTAL_ALGORITMOS; a++) {
        fprintf(f, "%s;%s;%s", ALGORITMOS[a].categoria, ALGORITMOS[a].nome,
                ALGORITMOS[a].complexidade_media);
        for (int i = 0; i < quantidade; i++) {
            fputc(';', f);
            escrever_decimal(f, pontos[i].media_ms[a]);
        }
        fputc('\n', f);
    }
    fclose(f);
}

static void mostrar_mais_rapidos(const Ponto *pontos, int quantidade, int primeiro_algoritmo,
                                 int por_k)
{
    printf("\nMenor tempo medio em cada ponto (diferencas pequenas podem ser ruido):\n");
    for (int i = 0; i < quantidade; i++) {
        int melhor = primeiro_algoritmo;
        for (int a = primeiro_algoritmo + 1; a < TOTAL_ALGORITMOS; a++)
            if (pontos[i].media_ms[a] < pontos[i].media_ms[melhor])
                melhor = a;
        printf("  %s = %-9d -> %-15s (%.6f ms)\n", por_k ? "maximo" : "N",
               por_k ? pontos[i].valor_maximo : (int)pontos[i].n,
               ALGORITMOS[melhor].nome, pontos[i].media_ms[melhor]);
    }
}

// Procura quando os algoritmos O(n log n) passam o Insertion Sort
static void mostrar_ponto_de_virada(const Ponto *pontos, int quantidade)
{
    static const int comparados[] = {IDX_MERGE, IDX_QUICK, IDX_HEAP};
    printf("\nPonto de virada (crossover) em relacao ao Insertion Sort:\n");
    for (int c = 0; c < 3; c++) {
        int a = comparados[c];
        int primeiro = quantidade;
        while (primeiro > 0 &&
               pontos[primeiro - 1].media_ms[a] < pontos[primeiro - 1].media_ms[IDX_INSERTION])
            primeiro--;
        if (primeiro == quantidade)
            printf("  %-11s: nao superou de forma consistente ate o maior N testado.\n",
                   ALGORITMOS[a].nome);
        else if (primeiro == 0)
            printf("  %-11s: ja era mais rapido no menor N testado (%d).\n",
                   ALGORITMOS[a].nome, (int)pontos[0].n);
        else
            printf("  %-11s: entre N = %d e N = %d.\n", ALGORITMOS[a].nome,
                   (int)pontos[primeiro - 1].n, (int)pontos[primeiro].n);
    }
    printf("  (O valor exato depende do computador. Repita para confirmar.)\n");
}

static void executar_experimento(const char *nome, Ponto *pontos, int quantidade, uint64_t seed,
                                 int repeticoes, int primeiro_algoritmo, int por_k)
{
    printf("\n================ EXPERIMENTO: %s ================\n", nome);

    FILE *ambiente = abrir_saida("ambiente", nome, "txt");
    escrever_ambiente(ambiente, seed, repeticoes);
    fclose(ambiente);

    FILE *execucoes = abrir_saida("execucoes", nome, "csv");
    FILE *resumo = abrir_saida("resumo", nome, "csv");
    fprintf(execucoes, "Seed;N;Valor maximo;Vetores no lote;Hash;Algoritmo;Repeticao;Tempo (ms)\n");
    fprintf(resumo, "Seed;N;Valor maximo;Vetores no lote;Hash;Categoria;Algoritmo;"
                    "Complexidade media;Repeticoes;Media (ms);Minimo (ms);Maximo (ms);"
                    "Memoria auxiliar calculada (bytes)\n");

    for (int i = 0; i < quantidade; i++)
        medir_ponto(&pontos[i], seed, repeticoes, primeiro_algoritmo, execucoes, resumo);
    fclose(execucoes);
    fclose(resumo);

    salvar_tabela(nome, pontos, quantidade, primeiro_algoritmo, por_k);
    mostrar_mais_rapidos(pontos, quantidade, primeiro_algoritmo, por_k);
}

static void modo_tamanhos(const char *nome, const size_t *ns, int quantidade,
                          uint64_t seed, int repeticoes)
{
    Ponto pontos[MAX_PONTOS];
    for (int i = 0; i < quantidade; i++) {
        pontos[i].n = ns[i];
        pontos[i].valor_maximo = VALOR_MAXIMO;
        pontos[i].vetores = 1;
    }
    executar_experimento(nome, pontos, quantidade, seed, repeticoes, 0, 0);
}

// N pequeno: usa varios vetores e calcula o tempo por vetor
static void modo_crossover(uint64_t seed, int repeticoes)
{
    static const size_t ns[] = {5, 10, 20, 30, 50, 75, 100, 150, 200, 500, 1000};
    const int quantidade = (int)(sizeof(ns) / sizeof(ns[0]));
    Ponto pontos[MAX_PONTOS];
    for (int i = 0; i < quantidade; i++) {
        pontos[i].n = ns[i];
        pontos[i].valor_maximo = VALOR_MAXIMO;
        pontos[i].vetores = ELEMENTOS_CROSSOVER / ns[i];
    }
    executar_experimento("crossover", pontos, quantidade, seed, repeticoes, 0, 0);
    mostrar_ponto_de_virada(pontos, quantidade);
}

// Testa o efeito do valor maximo (amplitude K)
static void modo_amplitude(uint64_t seed, int repeticoes)
{
    static const int maximos[] = {100, 10000, 1000000, 10000000, 100000000};
    const int quantidade = (int)(sizeof(maximos) / sizeof(maximos[0]));
    Ponto pontos[MAX_PONTOS];
    for (int i = 0; i < quantidade; i++) {
        pontos[i].n = N_TESTE_K;
        pontos[i].valor_maximo = maximos[i];
        pontos[i].vetores = 1;
    }
    executar_experimento("amplitudeK", pontos, quantidade, seed, repeticoes,
                         PRIMEIRO_EFICIENTE, 1);
}

// Aquecimento antes das medicoes, sem contar o tempo
static void aquecer(void)
{
    enum { N = 2000 };
    int base[N], copia[N];
    AreaTrabalho area = criar_area(N, (size_t)VALOR_MAXIMO + 1);
    gerar_dados(base, N, UINT64_C(999), N, VALOR_MAXIMO);
    for (int a = 0; a < TOTAL_ALGORITMOS; a++) {
        memcpy(copia, base, sizeof(base));
        ALGORITMOS[a].ordenar(copia, N, &area);
    }
    liberar_area(&area);
}

static int ler_numero(const char *texto, uint64_t *saida)
{
    if (*texto == '\0')
        return 0;
    for (const char *c = texto; *c; c++)
        if (*c < '0' || *c > '9')
            return 0;
    errno = 0;
    unsigned long long v = strtoull(texto, NULL, 10);
    if (errno == ERANGE)
        return 0;
    *saida = (uint64_t)v;
    return 1;
}

static int comparar_tamanhos(const void *a, const void *b)
{
    size_t x = *(const size_t *)a, y = *(const size_t *)b;
    return (x > y) - (x < y);
}

static int ler_lista(const char *texto, size_t *ns, int *quantidade)
{
    char copia[512];
    if (strlen(texto) >= sizeof(copia) || texto[0] == '\0' || texto[0] == ',' ||
        texto[strlen(texto) - 1] == ',' || strstr(texto, ",,") != NULL)
        return 0;
    strcpy(copia, texto);
    int q = 0;
    for (char *item = strtok(copia, ","); item != NULL; item = strtok(NULL, ",")) {
        uint64_t v;
        if (q >= MAX_PONTOS || !ler_numero(item, &v) || v < 1 || v > N_LIMITE)
            return 0;
        ns[q++] = (size_t)v;
    }
    qsort(ns, (size_t)q, sizeof(size_t), comparar_tamanhos);
    for (int i = 1; i < q; i++)
        if (ns[i] == ns[i - 1])
            return 0;
    *quantidade = q;
    return q > 0;
}

static int ler_linha(char *saida, size_t tamanho)
{
    char linha[256];
    if (fgets(linha, sizeof(linha), stdin) == NULL)
        return 0;
    size_t j = 0;
    for (size_t i = 0; linha[i] && j + 1 < tamanho; i++)
        if (linha[i] != ' ' && linha[i] != '\t' && linha[i] != '\r' && linha[i] != '\n')
            saida[j++] = linha[i];
    saida[j] = '\0';
    return 1;
}

static uint64_t criar_seed_aleatoria(void)
{
    uint64_t estado = (uint64_t)time(NULL) ^ (uint64_t)(agora_segundos() * 1e9);
    return proximo_aleatorio(&estado) % UINT64_C(1000000000);
}

static void mostrar_ajuda(const char *programa)
{
    printf("Uso: %s [--modo MODO] [--seed NUMERO|aleatoria] [--reps R] [--ns LISTA]\n\n", programa);
    printf("Modos:\n");
    printf("  rapido     N = 100, 1000, 10000, 50000, 100000\n");
    printf("  principal  N = 100, 1000, 10000, 50000, 100000\n");
    printf("  crossover  N pequenos, para achar o ponto de virada\n");
    printf("  k          efeito da amplitude dos valores\n");
    printf("  completo   principal + crossover + k\n");
    printf("  teste      somente o autoteste de correcao\n\n");
    printf("--seed  padrao: %" PRIu64 "\n", SEED_PADRAO);
    printf("--reps  repeticoes, de %d a %d (padrao %d)\n",
           REPETICOES_MINIMAS, REPETICOES_MAXIMAS, REPETICOES_MINIMAS);
    printf("--ns    troca os tamanhos do modo principal. Ex.: --ns 1000,5000,20000,50000,80000\n");
}

static int modo_valido(const char *m)
{
    return !strcmp(m, "rapido") || !strcmp(m, "principal") || !strcmp(m, "crossover") ||
           !strcmp(m, "k") || !strcmp(m, "completo") || !strcmp(m, "teste");
}

// Programa principal
int main(int argc, char **argv)
{
    const char *modo = NULL;
    uint64_t seed = SEED_PADRAO;
    int repeticoes = REPETICOES_MINIMAS;
    size_t ns[MAX_PONTOS] = {100, 1000, 10000, 50000, 100000};
    int quantidade_ns = 5;
    int ns_personalizado = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--ajuda") || !strcmp(argv[i], "--help")) {
            mostrar_ajuda(argv[0]);
            return 0;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Opcao incompleta ou desconhecida: %s (use --ajuda)\n", argv[i]);
            return EXIT_FAILURE;
        }
        const char *opcao = argv[i], *valor = argv[++i];
        uint64_t numero;
        if (!strcmp(opcao, "--modo") && modo_valido(valor)) {
            modo = valor;
        } else if (!strcmp(opcao, "--seed") && !strcmp(valor, "aleatoria")) {
            seed = criar_seed_aleatoria();
        } else if (!strcmp(opcao, "--seed") && ler_numero(valor, &numero)) {
            seed = numero;
        } else if (!strcmp(opcao, "--reps") && ler_numero(valor, &numero) &&
                   numero >= REPETICOES_MINIMAS && numero <= REPETICOES_MAXIMAS) {
            repeticoes = (int)numero;
        } else if (!strcmp(opcao, "--ns") && ler_lista(valor, ns, &quantidade_ns)) {
            ns_personalizado = 1;
        } else {
            fprintf(stderr, "Opcao ou valor invalido: %s %s (use --ajuda)\n", opcao, valor);
            return EXIT_FAILURE;
        }
    }
    if (ns_personalizado && modo != NULL && strcmp(modo, "principal") &&
        strcmp(modo, "completo")) {
        fprintf(stderr, "--ns so vale para os modos principal e completo.\n");
        return EXIT_FAILURE;
    }

    printf("=====================================================\n");
    printf("  ANALISE COMPARATIVA DE ALGORITMOS DE ORDENACAO\n");
    printf("=====================================================\n");

    // Mostra o menu se o modo nao for informado
    if (modo == NULL) {
        char texto[64];
        printf("\n 1) Teste com N = 100, 1000, 10000, 50000 e 100000\n");
        printf(" 2) Experimento principal (N = 100 ate 100000)\n");
        printf(" 3) Ponto de virada com N pequeno (crossover)\n");
        printf(" 4) Efeito da amplitude K\n");
        printf(" 5) Completo (opcoes 2 + 3 + 4)\n");
        printf(" 6) Somente autoteste de correcao\n");
        printf("Escolha [1-6]: ");
        fflush(stdout);
        if (!ler_linha(texto, sizeof(texto)) || strlen(texto) != 1 ||
            texto[0] < '1' || texto[0] > '6') {
            fprintf(stderr, "\nOpcao invalida. Execute no terminal e digite de 1 a 6.\n");
            return EXIT_FAILURE;
        }
        static const char *opcoes[] = {"rapido", "principal", "crossover", "k", "completo", "teste"};
        modo = opcoes[texto[0] - '1'];
        if (ns_personalizado && strcmp(modo, "principal") && strcmp(modo, "completo")) {
            fprintf(stderr, "--ns so vale para as opcoes 2 e 5.\n");
            return EXIT_FAILURE;
        }

        int seed_informada = 0;
        for (int i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--seed"))
                seed_informada = 1;
        if (!seed_informada && strcmp(modo, "teste")) {
            printf("\nSeed: ENTER = padrao (%" PRIu64 "), um numero, ou 'a' = aleatoria: ",
                   SEED_PADRAO);
            fflush(stdout);
            if (!ler_linha(texto, sizeof(texto))) {
                fprintf(stderr, "\nNao foi possivel ler a seed.\n");
                return EXIT_FAILURE;
            }
            if (!strcmp(texto, "a") || !strcmp(texto, "A")) {
                seed = criar_seed_aleatoria();
            } else if (texto[0] != '\0' && !ler_numero(texto, &seed)) {
                fprintf(stderr, "Seed invalida: use apenas digitos.\n");
                return EXIT_FAILURE;
            }
        }
    }

    printf("\n");
    if (!executar_autoteste()) {
        fprintf(stderr, "Experimento cancelado: ha algoritmo incorreto.\n");
        return EXIT_FAILURE;
    }
    if (!strcmp(modo, "teste"))
        return 0;

    time_t t = time(NULL);
    if (strftime(carimbo, sizeof(carimbo), "%Y%m%d_%H%M%S", localtime(&t)) == 0)
        strcpy(carimbo, "sem_data");

    printf("\n>>> SEED UTILIZADA: %" PRIu64 " <<<\n", seed);
    printf("Para repetir com os mesmos vetores: --modo %s --seed %" PRIu64 " --reps %d",
           modo, seed, repeticoes);
    if (ns_personalizado) {
        printf(" --ns ");
        for (int i = 0; i < quantidade_ns; i++)
            printf("%s%d", i ? "," : "", (int)ns[i]);
    }
    printf("\n\n");
    escrever_ambiente(stdout, seed, repeticoes);
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
    printf("\n*** AVISO: compilado sem otimizacao. Para o relatorio, use -O2. ***\n");
#endif

    aquecer();

    if (!strcmp(modo, "rapido")) {
        static const size_t rapidos[] = {100, 1000, 10000, 50000, 100000};
        modo_tamanhos("rapido", rapidos, 5, seed, repeticoes);
    }
    if (!strcmp(modo, "crossover") || !strcmp(modo, "completo"))
        modo_crossover(seed, repeticoes);
    if (!strcmp(modo, "k") || !strcmp(modo, "completo"))
        modo_amplitude(seed, repeticoes);
    if (!strcmp(modo, "principal") || !strcmp(modo, "completo")) {
        if (quantidade_ns < 5)
            printf("\n*** AVISO: o projeto exige no minimo 5 tamanhos de N. ***\n");
        if (ns[quantidade_ns - 1] >= 200000)
            printf("\nATENCAO: com N grande, Bubble e Selection podem levar muitos minutos.\n"
                   "Deixe o computador na tomada, sem suspender, e feche outros programas.\n");
        modo_tamanhos("principal", ns, quantidade_ns, seed, repeticoes);
    }

    printf("\nConcluido. Os arquivos foram salvos na pasta atual do terminal.\n");
    return 0;
}