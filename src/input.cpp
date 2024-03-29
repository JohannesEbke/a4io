/// a4::io::A4Input A4Input maps files to streams.

#include <a4/input.h>

#include <unordered_set>

#include <functional>
#include <string>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>

#include <a4/input_stream.h>

namespace a4 {
namespace io {


typedef boost::unique_lock<boost::mutex> Lock;

A4Input::A4Input(std::string name) {}

/// Add a stream to be processed, Returns this object again.
A4Input& A4Input::add_stream(shared<InputStream> s) {
    _streams.push_back(s);
    _ready.push_front(s.get());
    return *this;
}

/// Add a file to be processed, Returns this object again.
A4Input& A4Input::add_file(const std::string& filename, bool check_duplicates) {
    if (check_duplicates and _filenames_set.count(filename))
        FATAL("Duplicate input! '", filename, "' has already been add_file'd");
    _filenames_set.insert(filename);
    _filenames.push_back(filename);
    return *this;
}

InputStream* A4Input::pop_file() {
    if (_filenames.empty())
        return NULL;
    std::string filename = _filenames.front();
    _filenames.pop_front();
    shared<InputStream> s(new InputStream(filename));
    _streams.push_back(s);
    return s.get();
}

/// Callback executed when a stream is deleted.
/// Collates errors, reschedules unfinished streams.
void A4Input::report_finished(A4Input* input, InputStream* _s) {
    Lock l2(input->_mutex);
    
    if (_s->end()) {
        assert(input->_processing.erase(_s) == 1);
        input->_finished.insert(_s);
        VERBOSE("Finished processing ", _s->str());
        _s->close();
    } else if (_s->error() || input->_resched_count[_s] > 0) {
        ERROR("Encountered an error during reading: ", _s->str());
        
        input->_error.insert(_s);
        
    } else {
        WARNING("Finished but not fully processed (rescheduling): ", _s->str());
        
        input->_ready.push_front(_s);
        input->_resched_count[_s]++;
    }
}

/// Get a stream for processing, returns NULL if none are left (threadsafe).
shared<InputStream> A4Input::get_stream() {
    Lock lock(_mutex);
    InputStream* s = NULL;
    if (!_ready.empty()) {
        s = _ready.back();
        
        _ready.pop_back();
    } else if (!_filenames.empty()) {
        s = pop_file();
    } else {
        return shared<InputStream>();
    }

    _processing.insert(s);

    VERBOSE("Starting to process ", s->str());
    
    #if defined(HAVE_LAMBDA) and defined(HAVE_NOEXCEPT)
    auto cb = [&](InputStream* x) noexcept { report_finished(this, x); };
    #else
    std::function<void (InputStream*)> cb =
        std::bind(&A4Input::report_finished, this, std::placeholders::_1);
    #endif
    
    return shared<InputStream>(s, cb);
}

}
}
