extern "C"
int run_list_tests(int argc, char **argv);
/* Mixing C and C++ requires compiling main as C++. */
int main(int argc, char **argv){
    return run_list_tests(argc, argv);
}
