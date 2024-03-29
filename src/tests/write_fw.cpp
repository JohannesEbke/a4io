#include <iostream>

#include <a4/output_stream.h>
#include <a4/io/A4Stream.pb.h>

using namespace std;
using namespace a4::io;

int main(int argc, char ** argv) {
    std::string fn;
    if (argc == 1) {
        fn = "test_fw.a4";
    } else if (argc == 2) {
        fn = argv[1];
    } else assert(argc <= 2);

    OutputStream w(fn, "TestEvent");
    w.set_forward_metadata();

    TestMetaData m;
    m.set_meta_data(1);
    w.metadata(m);
    const int N = 500;
    TestEvent e;
    for(int i = 0; i < N; i++) {
        e.set_event_number(1000 + i);
        w.write(e);
    }
    m.set_meta_data(2);
    w.metadata(m);
    for(int i = 0; i < N; i++) {
        e.set_event_number(2000 + i);
        w.write(e);
    }
}
