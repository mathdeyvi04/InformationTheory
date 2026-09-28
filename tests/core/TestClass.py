from abc import ABC, abstractmethod
from pathlib import Path, PosixPath
from subprocess import run
from shutil import rmtree
from csv import writer, DictReader

class TestClass(ABC):

    ALGORITHM_NAMES = [
        "Huffman",
        "Shannon-Elias",
        "LempelZivWelch"
    ]

    def __init__(self):
        self.test_dir = Path(__file__).resolve().parent.parent
        self.list_path_to_test_files = self.get_test_files()
        self._path_to_executable = str(self.test_dir.parent / "bin/Compressor")
        self.trash_dir = self.test_dir / "trash"

    @staticmethod
    def get_size(path: PosixPath | Path) -> int:
        return path.stat().st_size

    @staticmethod
    def delete(path: PosixPath | Path):
        path.unlink(missing_ok=True)

    @staticmethod
    def clear_dir(path:PosixPath | Path):
        rmtree(path, ignore_errors=True)
        path.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def dump_results(exit_path: Path, results: list):

        with open(exit_path, "w", newline="", encoding="utf-8") as f:
            researcher = writer(f)
            researcher.writerows(results)

    @staticmethod
    def load_results(exit_path: Path):
        with exit_path.open("r", encoding="utf-8", newline="") as f:
            reader = DictReader(f)
            cabecalho = reader.fieldnames
            linhas = list(reader)

        return cabecalho, linhas

    def get_test_files(self) -> list[Path]:
        test_files_dir = self.test_dir / "test_files"
        return [f for f in test_files_dir.iterdir() if f.is_file()]

    def execute(
            self,
            input_file: Path | PosixPath,
            output_file: Path | PosixPath,
            algorithm: int,
            mode: int,
            get_stdout: bool = False,
            prefix: list[str] | None = None,
    ):
        command = [
            *(prefix or []),
            self._path_to_executable,
            "-i", str(input_file),
            "-o", str(output_file),
            "-n", str(algorithm),
            "-m", str(mode),
        ]

        return run(
            command,
            capture_output=get_stdout,
            text=get_stdout,
            check=True,
        )

    @abstractmethod
    def do_test(self):
        pass

    @abstractmethod
    def show_results(self):
        pass