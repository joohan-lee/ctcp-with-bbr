# Read the content from the file
with open('ctcp_tests_result.txt', 'r') as file:
    log_messages = file.read()

# Split the log messages into lines and extract the test results
lines = log_messages.strip().split('\n')
test_results = [line.strip() for line in lines[3:-1]]
score_results = [line.strip() for line in lines if line.startswith('SCORE:')]

# Count the total number of tests
total_tests = len(test_results)

# Count the number of passing tests
passing_tests = [result.strip() for result in test_results if result.endswith('PASS')]
fail_tests = [result.strip() for result in test_results if result.endswith('FAIL')]
print("fail_tests: {}".format(fail_tests))

# Calculate the average score
num_total_tests = len(passing_tests) + len(fail_tests)
average_score = (float(len(passing_tests)) / num_total_tests) * 100

# Print the results
print("Number of test sets(27 test cases in a set): {}".format(len(score_results)))
print("Number of test cases: {}".format(num_total_tests))
print("Number of passing test cases: {}({})".format(len(passing_tests), num_total_tests))
print("Number of fail test cases: {}({})".format(len(fail_tests), num_total_tests))
print("Average score: {:.4f}%".format(average_score))
