from tests.core.TestClass import TestClass
from matplotlib import pyplot as plt
from pathlib import Path

class MemoryUsage(TestClass):

    VALGRIND = "valgrind"

    def __init__(self):
        super().__init__()
        self.my_path = self.test_dir / "MemoryUsage"
        self.exit_dir = self.my_path / "results"
        self.exit_img = self.my_path / "results.png"

    # ─── Prefixo do Massif ──────────────────────────────────────────────
    def _massif_prefix(self, massif_out: Path) -> list[str]:
        return [
            self.VALGRIND,
            "--tool=massif",
            "--stacks=yes",
            "--time-unit=ms",
            "--max-snapshots=1000",
            "--detailed-freq=10",
            "--threshold=0.0",
            f"--massif-out-file={massif_out}",
        ]

    # ─── Parser do .massif.out ───────────────────────────────────────────
    @staticmethod
    def _parse_massif(path: Path) -> tuple[list[float], list[int], list[int]]:
        """
        Extrai os campos escalares de um arquivo .massif.out.

        Ignora o bloco heap_tree (que responde por ~99% das linhas).

        Retorna:
            times  — lista de instantes (na unidade definida no header)
            heaps  — mem_heap_B correspondente
            stacks — mem_stacks_B correspondente
        """
        times, heaps, stacks = [], [], []
        current: dict = {}

        with path.open("r", encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")

                if line.startswith("snapshot="):
                    if current:
                        times.append(current.get("time", 0.0))
                        heaps.append(current.get("mem_heap_B", 0))
                        stacks.append(current.get("mem_stacks_B", 0))
                    current = {}

                elif line.startswith("time="):
                    current["time"] = float(line.split("=", 1)[1])

                elif line.startswith("mem_heap_B="):
                    current["mem_heap_B"] = int(line.split("=", 1)[1])

                elif line.startswith("mem_stacks_B="):
                    current["mem_stacks_B"] = int(line.split("=", 1)[1])

                # heap_tree=... — ignorado (as linhas seguintes não casam
                # com nenhum dos prefixos acima, então caem fora naturalmente)

        if current:
            times.append(current.get("time", 0.0))
            heaps.append(current.get("mem_heap_B", 0))
            stacks.append(current.get("mem_stacks_B", 0))

        return times, heaps, stacks

    def do_test(self):
        # Se você realizar este teste, tome cuidado, pois demora MUITO.
        return
        self.clear_dir(self.exit_dir)
        for input_file in self.list_path_to_test_files:

            file_dir = self.exit_dir / input_file.name
            file_dir.mkdir(parents=True, exist_ok=True)

            for algorithm in range(len(self.ALGORITHM_NAMES)):

                name = self.ALGORITHM_NAMES[algorithm]

                compressed_file = file_dir / f"{name}.compressed"
                decompressed_file = file_dir / f"{name}.decompressed"

                compression_massif = file_dir / f"{name}.compression.massif.out"
                decompression_massif = file_dir / f"{name}.decompression.massif.out"

                # ─── Compressão ──────────────────────────────────────
                self.execute(
                    input_file,
                    compressed_file,
                    algorithm,
                    0,
                    prefix=self._massif_prefix(compression_massif),
                )

                # ─── Descompressão ───────────────────────────────────
                self.execute(
                    compressed_file,
                    decompressed_file,
                    algorithm,
                    1,
                    prefix=self._massif_prefix(decompression_massif),
                )

                # ─── Descarte dos intermediários ─────────────────────
                self.delete(compressed_file)
                self.delete(decompressed_file)

    # ─── Visualização ────────────────────────────────────────────────────
    def show_results(self) -> None:
        """
        Para cada arquivo de teste, gera uma figura com 2×N subplots:

            linha 0 → compressão   (uma coluna por algoritmo)
            linha 1 → descompressão (uma coluna por algoritmo)

        Cada subplot exibe duas curvas: heap e stack.
        """
        results_dir = self.my_path / "results"
        algorithms = self.ALGORITHM_NAMES

        if not results_dir.is_dir():
            print(f"Diretório de resultados não encontrado: {results_dir}")
            return

        for file_dir in sorted(results_dir.iterdir()):
            if not file_dir.is_dir():
                continue

            fig, axes = plt.subplots(
                2, len(algorithms),
                figsize=(5 * len(algorithms), 8),
                sharex=False,
                sharey=False,
                squeeze=False,
            )

            fig.suptitle(
                f"Uso de memória — {file_dir.name}",
                fontsize=14, fontweight="bold",
            )

            for col, alg in enumerate(algorithms):
                for row, mode in enumerate(("compression", "decompression")):
                    ax = axes[row][col]

                    massif = file_dir / f"{alg}.{mode}.massif.out"
                    if not massif.exists():
                        ax.set_visible(False)
                        continue

                    times, heaps, stacks = self._parse_massif(massif)
                    if not times:
                        ax.set_visible(False)
                        continue

                    # ─── Curva do heap ───────────────────────────────
                    ax.plot(
                        times, heaps,
                        label="Heap",
                        color="#4C72B0",
                        linewidth=1.5,
                    )

                    # ─── Curva do stack (apenas se houver dados) ─────
                    if any(s > 0 for s in stacks):
                        ax.plot(
                            times, stacks,
                            label="Stack",
                            color="#DD8452",
                            linewidth=1.5,
                        )

                    ax.set_title(f"{alg} — {mode}", fontsize=10)
                    ax.set_xlabel("Tempo (ms, sob Valgrind)", fontsize=9)
                    ax.set_ylabel("Bytes", fontsize=9)

                    # Escala log: cobre ordens de grandeza entre heap e stack.
                    # Zeros são clampeados para 1 (log não aceita 0).
                    ax.set_yscale("log")
                    ax.set_ylim(bottom=1)
                    ax.set_xscale("log")
                    ax.set_xlim(left=0.1)  # log não aceita 0

                    ax.grid(which="both", linestyle="--", alpha=0.35)
                    ax.set_axisbelow(True)
                    ax.legend(loc="upper left", fontsize=8)

            plt.tight_layout(rect=[0, 0, 1, 0.96])

            out = self.exit_dir / file_dir.name /f"{file_dir.name}.png"
            plt.savefig(out, dpi=150, bbox_inches="tight")
            plt.close(fig)  # libera memória; evita 8 janelas acumuladas
