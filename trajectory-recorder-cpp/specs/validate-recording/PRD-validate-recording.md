Add an executable that validates a session recorded using main_record.cpp . It should have the following operating modes:

1. full summary
- walk through the recording and summarize the statistics:
    - average number of gamepad inputs per second
    - dead period at the start and end of the recording (period until the first gamepad input, and after the last input)
    - list of all buttons pressed / axes moved and their frequency
    - (think of other useful statistics)
- should either take the path to a specific session, or an output folder containing multiple sessions
    - in the latter case, it should summarize all sessions consecutively


2. step mode
- step through the recording using the arrow keys (show the elapsed time since first frame)
- for each frame: print out the recorded gamepad inputs