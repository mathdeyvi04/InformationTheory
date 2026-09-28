from tests.core.TestClass import TestClass
from matplotlib.patches import Patch
from matplotlib import pyplot as plt
import numpy as np

class TimeDuration(TestClass):
    """
    O teste TimeDuration mede o tempo médio de compressão e descompressão
    de cada algoritmo sobre cada arquivo de referência. Para reduzir ruído,
    cada operação é executada cinco vezes, e a média dos tempos é registrada.
    """

    ITERATION_NUMBER_FOR_MEDIAN = 5

    def __init__(self):
        super().__init__()
        self.my_path = self.test_dir / "TimeDuration"
        self.exit_file = self.my_path / "results.csv"
        self.exit_img = self.my_path / "results.png"

    def do_test(self):

        first = True
        results: list[list[str | float]] = [["Nome", "Tamanho Original"]]
        for file in self.list_path_to_test_files:
            results.append([])
            results[-1].append(file.name)
            results[-1].append(file.stat().st_size)

            for i in range(0, len(TestClass.ALGORITHM_NAMES)):
                if first:
                    results[0].append(f"{TestClass.ALGORITHM_NAMES[i]}_C")
                    results[0].append(f"{TestClass.ALGORITHM_NAMES[i]}_D")

                path_to_output = self.trash_dir / f"{file.name}.out"

                # Vamos executar algumas vezes para conseguir uma média de tempo
                sum_time_compress = 0
                sum_time_decompress = 0
                for _ in range(0, TimeDuration.ITERATION_NUMBER_FOR_MEDIAN):
                    popout = self.execute(
                        file,
                        path_to_output,
                        i,
                        2,
                        True
                    )

                    elapsed_c, elapsed_d = map(lambda x: float(x), popout.stdout.replace("ms", "").split(" - "))
                    sum_time_compress += elapsed_c
                    sum_time_decompress += elapsed_d

                results[-1].append(sum_time_compress / TimeDuration.ITERATION_NUMBER_FOR_MEDIAN)
                results[-1].append(sum_time_decompress / TimeDuration.ITERATION_NUMBER_FOR_MEDIAN)

            first = False

        self.clear_dir(self.trash_dir)
        self.dump_results(self.exit_file, results)

    def show_results(self):
        self.apply_font()
        plt.style.use("dark_background")
        cabecalho, rows = self.load_results(self.exit_file)

        algorithms = TestClass.ALGORITHM_NAMES

        # ─── Estrutura: [(nome, tamanho, {alg: (t_compressao, t_descompressao)})] ──
        data = []
        for row in rows:
            nome = row["Nome"]
            tamanho = int(row["Tamanho Original"])

            # a.txt (1 B) tem tempos ~0.005 ms — invisíveis na escala do bible.txt
            if tamanho <= 1:
                continue

            tempos = {
                alg: (float(row[f"{alg}_C"]), float(row[f"{alg}_D"]))
                for alg in algorithms
            }

            data.append((nome, tamanho, tempos))

        data.sort(key=lambda d: d[1])

        nomes = [d[0] for d in data]
        tamanhos = [d[1] for d in data]

        rotulos = [
            f"{nome}\n({tam:,} B)".replace(",", ".")
            for nome, tam in zip(nomes, tamanhos)
        ]

        # ─── Cores por algoritmo ─────────────────────────────────────────────
        cmap = plt.get_cmap("tab10")
        cores = {alg: cmap(i) for i, alg in enumerate(algorithms)}

        # ─── Layout das barras: 2 por algoritmo (C, D) ───────────────────────
        n_alg = len(algorithms)
        n_barras = n_alg * 2
        largura_total = 0.85
        largura = largura_total / n_barras
        offset_inicial = -largura_total / 2 + largura / 2

        x = np.arange(len(data))

        # ─── Plot ────────────────────────────────────────────────────────────
        fig, ax = plt.subplots(figsize=(14, 6.5))

        for i, (_, _, tempos) in enumerate(data):
            for a_idx, alg in enumerate(algorithms):
                t_c, t_d = tempos[alg]
                cor = cores[alg]

                # Compressão — barra sólida
                ax.bar(
                    x[i] + offset_inicial + (a_idx * 2) * largura,
                    t_c,
                    largura,
                    color=cor,
                    edgecolor="black",
                    linewidth=0.5,
                )

                # Descompressão — barra hachurada
                ax.bar(
                    x[i] + offset_inicial + (a_idx * 2 + 1) * largura,
                    t_d,
                    largura,
                    color=cor,
                    edgecolor="black",
                    linewidth=0.5,
                    hatch="///",
                )

        # ─── Eixos e estilo ──────────────────────────────────────────────────
        ax.set_yscale("log")
        ax.set_ylim(1e-1, 2e4)

        ax.set_xticks(x)
        ax.set_xticklabels(rotulos, fontsize=9)
        ax.set_xlabel("Arquivo (tamanho original em bytes)", fontsize=11)
        ax.set_ylabel("Tempo de execução (ms, escala log)", fontsize=11)
        ax.set_title(
            "Comparação de Tempo de Compressão e Descompressão",
            fontsize=13, fontweight="bold",
        )

        ax.grid(axis="y", which="major", linestyle="--", alpha=0.4)
        ax.grid(axis="y", which="minor", linestyle=":", alpha=0.2)
        ax.set_axisbelow(True)

        # ─── Legenda 1: algoritmos (cores) ───────────────────────────────────
        handles_alg = [
            Patch(facecolor=cores[alg], edgecolor="black", label=alg)
            for alg in algorithms
        ]

        legenda_alg = ax.legend(
            handles=handles_alg,
            title="Algoritmo",
            loc="upper left",
            framealpha=0.95,
        )
        ax.add_artist(legenda_alg)  # preserva a 1ª legenda

        # ─── Legenda 2: operações (hachuras) ─────────────────────────────────
        handles_op = [
            Patch(facecolor="white", edgecolor="black", label="Compressão"),
            Patch(facecolor="white", edgecolor="black", hatch="///", label="Descompressão"),
        ]

        ax.legend(
            handles=handles_op,
            title="Operação",
            loc="upper right",
            framealpha=0.95,
        )

        # ─── Salvamento e exibição ───────────────────────────────────────────
        plt.tight_layout()
        plt.savefig(self.exit_img, dpi=150, bbox_inches="tight")
        plt.show()