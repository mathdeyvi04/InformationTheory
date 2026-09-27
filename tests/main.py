from tests.Compressibility.Compressibility import Compressibility
from tests.MaxCompressibility.MaxCompressibility import MaxCompressibility
from tests.TimeDuration.TimeDuration import TimeDuration
import argparse

class TestManager:

    POSSIBLES_TESTS = [
        Compressibility,
        MaxCompressibility,
        TimeDuration
    ]

    def __init__(self, test_number: int = 0):

        if test_number > len(TestManager.POSSIBLES_TESTS):
            return

        TestToRealize = TestManager.POSSIBLES_TESTS[test_number]()
        TestToRealize.do_test()
        TestToRealize.show_results()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Meu programa de exemplo"
    )

    parser.add_argument(
        "-n", "--number",
        type=int,
        required=True,
        help="Número do algoritmo a executar"
    )

    args = parser.parse_args()
    TestManager(args.number)
