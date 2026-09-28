from tests.core.TestClass import TestClass
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D
import numpy as np
import math

class MaxCompressibility(TestClass):
    """
    O teste MaxCompressibility investiga até que ponto a recompressão
    iterativa ainda é vantajosa. Para cada arquivo de referência,
    o teste aplica cada algoritmo consecutivamente, alimentando
    sempre a saída de uma execução como entrada da próxima. Após
    cada iteração, o tamanho do arquivo resultante é registrado,
    permitindo observar a evolução do tamanho ao longo das execuções.
    """

    ITERATION_NUMBER = 5

    def __init__(self):
        super().__init__()
        self.my_path = self.test_dir / "MaxCompressibility"
        self.exit_file = self.my_path / "results.csv"
        self.exit_img = self.my_path / "results.png"

    def do_test(self):

        results: list[list[str | int]] = [["Nome", "Tamanho"]]
        first = True
        for file in self.list_path_to_test_files:
            results.append([])
            results[-1].append(file.name)
            results[-1].append(file.stat().st_size)

            # Executamos cada algoritmo uma quantidade de vezes e gravamos o tamanho do arquivo após
            for algorithm_number in range(0, len(TestClass.ALGORITHM_NAMES)):
                file_to_compress = file
                for iteration in range(0, MaxCompressibility.ITERATION_NUMBER):
                    if first:
                        # Apenas para consertarmos o cabeçalho
                        results[0].append(
                            f"{TestClass.ALGORITHM_NAMES[algorithm_number]}i{iteration + 1}"
                        )

                    path_to_output = self.trash_dir / f"{file.name}i{iteration + 1}.out"

                    self.execute(
                        file_to_compress,
                        path_to_output,
                        algorithm_number,
                        0
                    )

                    results[-1].append(self.get_size(path_to_output))
                    file_to_compress = path_to_output

            first = False

        # Limpamos todos os arquivos criados
        self.clear_dir(self.trash_dir)

        # Salvamos as informações obtidas
        self.dump_results(self.exit_file, results)

    def show_results(self):
        self.apply_font()

        cabecalho, rows = self.load_results(self.exit_file)

        algorithms = TestClass.ALGORITHM_NAMES
        n_iter = MaxCompressibility.ITERATION_NUMBER

        # ─── Estrutura: [(nome, tamanho_inicial, {alg: [s1, ..., sN]})] ──────
        data = []
        for row in rows:
            nome = row["Nome"]
            tamanho = int(row["Tamanho"])
            series = {}

            for alg in algorithms:
                series[alg] = [
                    int(row[f"{alg}i{it}"])
                    for it in range(1, n_iter + 1)
                ]

            data.append((nome, tamanho, series))

        data.sort(key=lambda d: d[1])

        # ─── Marcadores por algoritmo ────────────────────────────────────────
        marcadores = ["o", "s", "^", "D", "v", "P", "*", "X", "<", ">"]
        marcador_por_alg = {
            alg: marcadores[i % len(marcadores)]
            for i, alg in enumerate(algorithms)
        }

        # ─── Grade de subplots (um por arquivo) ──────────────────────────────
        n_arquivos = len(data)
        n_cols = 4
        n_rows = math.ceil(n_arquivos / n_cols)

        fig, axes = plt.subplots(
            n_rows, n_cols,
            figsize=(4.2 * n_cols, 3.4 * n_rows),
            sharex=True,
            sharey=False,
            squeeze=False,
        )

        # Eixo X: 0 (original) + 1..N (iterações)
        iteracoes = np.arange(0, n_iter + 1)

        for idx, (nome, tamanho, series) in enumerate(data):
            r, c = divmod(idx, n_cols)
            ax = axes[r][c]

            for alg in algorithms:
                # Ponto 0 = tamanho original; pontos 1..N = após cada compressão
                valores = [tamanho] + series[alg]

                ax.plot(
                    iteracoes,
                    valores,
                    marker=marcador_por_alg[alg],
                    linestyle="-",
                    linewidth=1.4,
                    markersize=6,
                    markerfacecolor="white",
                    markeredgewidth=1.4,
                    label=alg.capitalize(),
                )

            ax.set_yscale("log")
            ax.set_title(
                f"{nome}\n({tamanho:,} B)".replace(",", "."),
                fontsize=10, fontweight="bold",
            )
            ax.grid(axis="y", which="major", linestyle="--", alpha=0.4)
            ax.grid(axis="y", which="minor", linestyle=":", alpha=0.2)
            ax.set_axisbelow(True)
            ax.set_xticks(iteracoes)
            ax.set_xlim(-0.3, n_iter + 0.3)

        # Esconde eixos vazios (se n_arquivos não preencher a grade)
        for idx in range(n_arquivos, n_rows * n_cols):
            r, c = divmod(idx, n_cols)
            axes[r][c].set_visible(False)

        # ─── Rótulos compartilhados ──────────────────────────────────────────
        for c in range(n_cols):
            axes[n_rows - 1][c].set_xlabel("Iteração", fontsize=9)

        for r in range(n_rows):
            axes[r][0].set_ylabel("Tamanho (bytes, log)", fontsize=9)

        # ─── Título geral ────────────────────────────────────────────────────
        fig.suptitle(
            "\n\nEvolução de Compressão Iterativa",
            fontsize=14, fontweight="bold",
            y=1.02,
        )

        # ─── Legenda única (algoritmos) compartilhada ────────────────────────
        handles_algoritmos = [
            Line2D(
                [0], [0],
                color="black",
                marker=marcador_por_alg[alg],
                linestyle="-",
                linewidth=1.4,
                markersize=7,
                markerfacecolor="white",
                markeredgecolor="black",
                markeredgewidth=1.4,
                label=alg,
            )
            for alg in algorithms
        ]

        fig.legend(
            handles=handles_algoritmos,
            title="Algoritmo",
            loc="upper right",
            bbox_to_anchor=(0.99, 1.0),
            framealpha=0.95,
            fontsize=9,
            title_fontsize=10,
        )

        plt.tight_layout(rect=[0.0, 0.0, 1.0, 0.98])
        plt.savefig(self.exit_img, dpi=150, bbox_inches="tight")
        plt.show()
