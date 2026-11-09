"""Report the standalone suite's PASS/FAIL lines through PlatformIO."""
from platformio.test.result import TestCase, TestStatus
from platformio.test.runners.base import TestRunnerBase


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        if line.startswith("PASS "):
            self.test_suite.add_case(TestCase(line[5:].strip(), TestStatus.PASSED))
        elif line.startswith("FAIL "):
            self.test_suite.add_case(TestCase(line[5:].strip(), TestStatus.FAILED))
