from tests.core.TestClass import TestClass
from matplotlib import pyplot as plt
import numpy as np

class Compressibility(TestClass):
    """
    O teste Compressibility avalia a taxa de compressão dos algoritmos implementados
    sobre um conjunto de arquivos de referência.
    Após cada execução, o arquivo temporário é descartado para não poluir o ambiente.
    Ao final, os resultados são consolidados em um arquivo CSV (results.csv), servindo
    de entrada para a posterior geração de gráficos comparativos (results.png).
    """

    def __init__(self):
        super().__init__()
        self.my_path = self.test_dir / "Compressibility"
        self.exit_file = self.my_path / "results.csv"
        self.exit_img = self.my_path / "results.png"

    def do_test(self):

        results: list[list[str | float]] = [["Nome", "Tamanho Original", *TestClass.ALGORITHM_NAMES]]
        for file in self.list_path_to_test_files:
            results.append([])
            # Salvamos a informação do nome
            results[-1].append(file.name)

            # Salvamos a informação do tamanho original
            results[-1].append(self.get_size(file))

            for algorithm_number in range(0, len(TestClass.ALGORITHM_NAMES)):
                path_to_output = self.trash_dir / f"{file.name}{algorithm_number}.out"

                self.execute(
                    file,
                    path_to_output,
                    algorithm_number,
                    0
                )

                # Salvamos a informação da razão de compressão
                results[-1].append(self.get_size(path_to_output) / results[-1][1])

        # Para não poluirmos o ambiente, vamos jogar no lixo
        self.clear_dir(self.trash_dir)

        # Salvamos as informações obtidas
        self.dump_results(self.exit_file, results)

    def show_results(self):

        cabecalho, linhas = self.load_results(self.exit_file)
        # As duas primeiras colunas são metadados; o resto são algoritmos
        coluna_arquivo = cabecalho[0]
        coluna_tamanho = cabecalho[1]
        algoritmos = cabecalho[2:]  # dinâmico: pega todas as demais

        # ─── Estrutura dos dados ──────────────────────────────────────────────────────
        # Cada elemento: (nome, tamanho, {algoritmo: taxa})
        dados = []

        for linha in linhas:
            nome = linha[coluna_arquivo]
            tamanho = int(linha[coluna_tamanho])

            # Arquivos minúsculos (1 byte) só expandem: distorcem o eixo 0–1
            if tamanho <= 1:
                continue

            taxas = {alg: float(linha[alg]) for alg in algoritmos}
            dados.append((nome, tamanho, taxas))

        # Ordena por tamanho crescente (revela tendência)
        dados.sort(key=lambda d: d[1])

        nomes = [d[0] for d in dados]
        tamanhos = [d[1] for d in dados]

        # ─── Rótulos: nome + tamanho formatado ────────────────────────────────────────
        rotulos = [
            f"{nome}\n({tam:,} B)".replace(",", ".")
            for nome, tam in zip(nomes, tamanhos)
        ]

        # ─── Plot ─────────────────────────────────────────────────────────────────────
        x = np.arange(len(nomes))

        # Largura das barras calculada para caber qualquer número de algoritmos
        LARGURA_TOTAL = 0.8
        n_alg = len(algoritmos)
        largura = LARGURA_TOTAL / n_alg
        offset_inicial = -LARGURA_TOTAL / 2 + largura / 2

        # Paleta com cor por algoritmo (tab10 suporta até 10; generalize se precisar)
        cmap = plt.get_cmap("tab10")
        cores = {alg: cmap(i % 10) for i, alg in enumerate(algoritmos)}

        fig, ax = plt.subplots(figsize=(12, 6))

        for i, alg in enumerate(algoritmos):
            offset = offset_inicial + i * largura
            valores = [d[2][alg] for d in dados]

            barras = ax.bar(
                x + offset,
                valores,
                largura,
                label=alg,
                color=cores[alg],
                edgecolor="black",
                linewidth=0.5,
            )

            # Anotação no topo de cada barra
            for barra in barras:
                altura = barra.get_height()
                ax.annotate(
                    f"{altura:.3f}",
                    xy=(barra.get_x() + barra.get_width() / 2, altura),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha="center", va="bottom",
                    fontsize=8,
                )

        # ─── Eixos e estilo ───────────────────────────────────────────────────────────
        ax.set_ylabel("Taxa de compressão", fontsize=11)
        ax.set_xlabel("Arquivo (tamanho em bytes)", fontsize=11)
        ax.set_title("Comparação de Taxa de Compressão",
                     fontsize=13, fontweight="bold")

        ax.set_xticks(x)
        ax.set_xticklabels(rotulos, fontsize=9)
        ax.set_ylim(0, 1.0)
        ax.legend(title="Algoritmo", loc="upper right")
        ax.grid(axis="y", linestyle="--", alpha=0.4)
        ax.set_axisbelow(True)

        plt.tight_layout(rect=[0.0, 0.04, 1.0, 1.0])
        plt.savefig(self.exit_img, dpi=150, bbox_inches="tight")
        plt.show()