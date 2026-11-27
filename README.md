# Análise Comparativa de Algoritmos de Ordenação

Projeto desenvolvido para a disciplina de **Estrutura de Dados I**, do Instituto Federal de Educação, Ciência e Tecnologia de São Paulo (IFSP), com o objetivo de implementar e comparar o desempenho de diferentes algoritmos de ordenação.

A proposta é analisar como o tamanho dos dados influencia o tempo de execução dos algoritmos, considerando suas complexidades computacionais e características de funcionamento.

## Objetivo do projeto

Investigar a seguinte questão:

> A escolha do algoritmo de ordenação mais adequado depende do tamanho dos dados a serem processados?

Para responder a essa questão, foram implementados nove algoritmos de ordenação em linguagem C, divididos em duas categorias.

## Algoritmos implementados

### Baseados em comparação

- **Bubble Sort:** compara elementos adjacentes e realiza trocas quando necessário.
- **Selection Sort:** seleciona o menor elemento e o posiciona na posição correta.
- **Insertion Sort:** insere cada elemento em sua posição adequada.
- **Merge Sort:** utiliza a estratégia de divisão e conquista.
- **Quick Sort:** utiliza um pivô para dividir e ordenar os elementos.
- **Heap Sort:** utiliza uma estrutura Heap para organizar os elementos.

### Não baseados em comparação

- **Counting Sort:** utiliza contadores para organizar os valores.
- **Radix Sort:** realiza a ordenação considerando os dígitos dos números.
- **Bucket Sort:** distribui os elementos em grupos (baldes) para realizar a ordenação.

## Tecnologias e ambiente

| Componente | Especificação |
|---|---|
| Linguagem | C |
| Padrão | C11 |
| Compilador | GCC 16.2.0 |
| Ambiente | MSYS2 UCRT64 |
| Otimização | -O2 |
| Editor | Visual Studio Code |
| Sistema operacional | Windows 10 Pro 22H2 |
| Processador | Intel Core i5-10400 |
| Memória RAM | 16 GB DDR4 2667 MHz |

## Metodologia dos testes

Os experimentos foram realizados utilizando vetores de números inteiros aleatórios, com valores entre 0 e 1.000.000.

Foram utilizados cinco tamanhos de entrada:

| Teste | Quantidade de elementos |
|---|---:|
| 1 | 100 |
| 2 | 1.000 |
| 3 | 10.000 |
| 4 | 50.000 |
| 5 | 100.000 |

Cada algoritmo foi executado três vezes para cada tamanho de vetor, utilizando uma seed fixa (`20261127`) para permitir a reprodução dos dados.

A medição do tempo foi realizada em milissegundos, utilizando a função `QueryPerformanceCounter`, da API do Windows.

Para garantir condições equivalentes, todos os algoritmos receberam cópias dos mesmos vetores. A geração dos dados, a alocação de memória, a cópia dos vetores e a verificação dos resultados foram realizadas fora do intervalo cronometrado.

A função `qsort`, da biblioteca padrão do C, foi utilizada somente para verificar a correção dos resultados, não participando das medições de desempenho.

## Resultados

Os testes demonstraram diferenças significativas entre os algoritmos, principalmente conforme aumentou a quantidade de elementos.

Para o vetor de 100.000 elementos, os resultados médios foram:

| Algoritmo | Tempo médio |
|---|---:|
| Bubble Sort | 27.716,825 ms |
| Selection Sort | 9.835,605 ms |
| Insertion Sort | 5.131,389 ms |
| Merge Sort | 10,631 ms |
| Quick Sort | 12,256 ms |
| Heap Sort | 16,251 ms |
| Counting Sort | 3,770 ms |
| Radix Sort | 1,852 ms |
| Bucket Sort | 3,538 ms |

**Destaque:** o Radix Sort apresentou o menor tempo de execução para a entrada de 100.000 elementos, com aproximadamente 1,85 ms.

Os resultados também evidenciaram o crescimento expressivo dos algoritmos de complexidade O(n²), enquanto os métodos mais eficientes apresentaram melhor desempenho para conjuntos maiores.

## Complexidade computacional

| Algoritmo | Complexidade |
|---|---|
| Bubble Sort | O(n²) |
| Selection Sort | O(n²) |
| Insertion Sort | O(n²) |
| Merge Sort | O(n log n) |
| Quick Sort | O(n log n) médio / O(n²) pior |
| Heap Sort | O(n log n) |
| Counting Sort | O(n + k) |
| Radix Sort | O(d × (n + k)) |
| Bucket Sort | O(n) médio / O(n²) pior |

## Estrutura do projeto

O repositório contém o código-fonte responsável pela implementação dos algoritmos, execução dos experimentos e geração dos resultados, além dos materiais utilizados para documentar a análise comparativa.

## Considerações finais

O desenvolvimento deste projeto permitiu comparar, na prática, diferentes algoritmos de ordenação e compreender a influência da complexidade computacional no desempenho.

Os resultados demonstraram que não existe um único algoritmo ideal para todas as situações. A escolha deve considerar o tamanho da entrada, as características dos dados, o tempo de execução e o consumo de memória.

## Observções

Os arquivos TXT e CSV são os arquivos gerados pelo autor que foram utilizados como base para estruturar o relatório técnico. 

## Autor

**Vinicius Ribeiro Nery Costa**

Instituto Federal de Educação, Ciência e Tecnologia de São Paulo (IFSP)

Disciplina: Estrutura de Dados I

## Repositório

[GitHub – Análise Comparativa de Algoritmos de Ordenação](https://github.com/ViniciusRibeiroNery/desempenho-algoritmos-ordenacao)
