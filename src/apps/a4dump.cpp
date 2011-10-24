#include <algorithm>

#include <limits>
using std::numeric_limits;

#include <utility>
#include <string>

#include <iostream>
#include <iomanip>

#include <map>
#include <unordered_map>

#include <boost/program_options.hpp>

#include <google/protobuf/text_format.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
using google::protobuf::DescriptorPool;
using google::protobuf::FileDescriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Message;
using google::protobuf::Reflection;

#include <a4/input.h>
#include <a4/message.h>

/// Collect statistics about variables
class StatsCollector {

    /// A class to measure the largest column width and then print an aligned
    /// table. Two passes need to be made over the data, one to measure,
    /// another to print
    class ColumnSizeMeasurer {
    public:
        ColumnSizeMeasurer() : current_index(0), measuring(true) {}
    
        std::vector<int> lengths;
        size_t current_index;
        bool measuring;
        
        void print(std::ostream& out) {}
        
        template<typename T, class... Args>
        void print(std::ostream& out, const T& value, const Args& ...args) {
            if (measuring) {
            
                std::stringstream stream;
                stream << std::fixed << value;
                const int this_length = stream.tellp();
                
                if (current_index >= lengths.size())
                    lengths.push_back(this_length);
                else {
                    if (lengths.at(current_index) < this_length)
                        lengths.at(current_index) = this_length;
                }
                
            } else
                out << std::setw(lengths.at(current_index)) << std::fixed << value << " ";
            
            current_index++;
            print(out, args...);
        }
        
        void newline(std::ostream& out) {
            current_index = 0;
            if (!measuring)
                out << std::endl;
        }
    };
    
    /// Collect stats about one variable
    class Stats {
    public:
        Stats() :
            n(0), total(0), sum_of_squares(0),
            min (numeric_limits<double>::max()),
            min1(numeric_limits<double>::max()),
            max (numeric_limits<double>::min()),
            max1(numeric_limits<double>::min()) {}
            
        double n;
        double total;
        double sum_of_squares;
        double min, min1;
        double max, max1;
        
        /// Collect one value
        /// Values aren't counted if they are the minimum or maximum. This is to
        /// avoid pulling the distribution towards any default values.
        void collect(const double& value) {
        
            if (value == -999 || value == -9999 || value == -99999)
                // Special case.
                return;
        
            // (or equal) to make sure don't follow second branch otherwise.
            if (value <= min) min = value;
            else if (value < min1) min1 = value;
            
            if (value >= max) max = value;
            else if (value > max1) max1 = value;
            
            // If min1/max1 aren't set, then we haven't seen many interesting 
            // values. Therefore we should include this value in the count.            
            if ((value != min && value != max) 
                || min1 == numeric_limits<double>::max() 
                || max1 == numeric_limits<double>::min()) {
                n++;
                total += value;
                sum_of_squares += value*value;
            }
        }
        
        double mean() const {
            return total / n;
        }
        
        double stddev() const {
            return sqrt((sum_of_squares - (mean()*mean())) / n);
        }
        
        void print(std::ostream& out, ColumnSizeMeasurer& csm) {
            if (n == 0) {
                csm.print(out, 0, 0, 0, 0, 0, 0, 0);
                return;
            }
            csm.print(out, n, mean(), stddev(), min, max, 
                      (min1 == numeric_limits<double>::max() ? min : min1), 
                      (max1 == numeric_limits<double>::min() ? max : max1));
        }
    };

public:

    std::unordered_map<const FieldDescriptor*, Stats> stats;
    
    /// Collect values from one message
    void collect(const Message& message) {
        const Reflection* reflection = message.GetReflection();
        
        std::vector<const FieldDescriptor*> fields;
        reflection->ListFields(message, &fields);
        foreach (const FieldDescriptor* field, fields) {


            if (field->is_repeated()) {
                int count = reflection->FieldSize(message, field);
                for (int j = 0; j < count; j++) {
            
                    switch (field->cpp_type()) {
#define HANDLE_TYPE(CPPTYPE, METHOD)                                           \
                    case FieldDescriptor::CPPTYPE_##CPPTYPE:                           \
                        stats[field].collect(reflection->GetRepeated##METHOD(message, field, j)); \
                        break;
                
                    // Only handle numeric types
                    HANDLE_TYPE(INT32 , Int32 );
                    HANDLE_TYPE(INT64 , Int64 );
                    HANDLE_TYPE(UINT32, UInt32);
                    HANDLE_TYPE(UINT64, UInt64);
                    HANDLE_TYPE(FLOAT , Float );
                    HANDLE_TYPE(DOUBLE, Double);
                    HANDLE_TYPE(BOOL  , Bool  );
#undef HANDLE_TYPE

                    case FieldDescriptor::CPPTYPE_MESSAGE:
                        collect(reflection->GetRepeatedMessage(message, field, j));
                        break;
                        
                    default:
                        break;
                    }
                }
            } else { // (not repeated)
            
                switch (field->cpp_type()) {
#define HANDLE_TYPE(CPPTYPE, METHOD)                                           \
                    case FieldDescriptor::CPPTYPE_##CPPTYPE:                           \
                        if (reflection->HasField(message, field))                      \
                            stats[field].collect(reflection->Get##METHOD(message, field)); \
                        break;
                
                    // Only handle numeric types
                    HANDLE_TYPE(INT32 , Int32 );
                    HANDLE_TYPE(INT64 , Int64 );
                    HANDLE_TYPE(UINT32, UInt32);
                    HANDLE_TYPE(UINT64, UInt64);
                    HANDLE_TYPE(FLOAT , Float );
                    HANDLE_TYPE(DOUBLE, Double);
                    HANDLE_TYPE(BOOL  , Bool  );
#undef HANDLE_TYPE

                    case FieldDescriptor::CPPTYPE_MESSAGE:
                        collect(reflection->GetMessage(message, field));
                        break;
                        
                    default:
                        break;
                }
            }
        }
    }
    
    friend std::ostream& operator<< (std::ostream& o, StatsCollector const& sc) {
        typedef std::pair<const FieldDescriptor*, Stats> StatsItem;
        
        std::vector<StatsItem> sorted_stats;
        
        foreach (StatsItem i, sc.stats)
            sorted_stats.push_back(i);            
        
        struct {
            bool operator() (const StatsItem& lhs, const StatsItem& rhs) { 
                return lhs.first < rhs.first;
                // return lhs.first->full_name() < rhs.first->full_name();
            }
        } StatsItemOrdering;
        
        std::sort(sorted_stats.begin(), sorted_stats.end(), StatsItemOrdering);
        
        ColumnSizeMeasurer csm;
        
        // Do two iterations, one to measure, other to print.
        for (int i = 0; i < 2; i++) {
            csm.print(o, "Variable", "n", "mean", "stddev", "min", "max", "min1", "max1");
            csm.newline(o);
        
            foreach (StatsItem i, sorted_stats) {
                o << std::left; // left align first column
                csm.print(o, i.first->full_name());
                o << std::right;
                i.second.print(o, csm);
                csm.newline(o);
            }
        
            csm.measuring = false;
        }
        
        return o;
    }
};

void dump_message(const Message& message, const std::vector<std::string>& vars) {
    if (vars.size()) {
        // specialized code
        throw a4::Fatal("Not implemented yet");
    } else {
        std::string str;
        google::protobuf::TextFormat::PrintToString(message, &str);
        std::cout << str << std::endl;
    }    
}

int main(int argc, char ** argv) {
    a4::Fatal::enable_throw_on_segfault();

    namespace po = boost::program_options;

    std::vector<std::string> input_files, variables;
    size_t event_count = -1, event_index = -1;
    bool collect_stats = false;
    
    po::positional_options_description p;
    p.add("input", -1);

    po::options_description commandline_options("Allowed options");
    commandline_options.add_options()
        ("help,h", "produce help message")
        ("event-index,i", po::value(&event_index)->default_value(0), "event to start dumping from (starts at 0)")
        ("count,c", po::value(&event_count)->default_value(1), "number to dump'")
        ("input", po::value(&input_files), "input file names (runs once per specified file)")
        ("var,v", po::value(&variables), "variables to dump (defaults to all)")
        ("collect-stats,S", po::value(&collect_stats), "should collect statistics for all numeric variables")
    ;
    
    po::variables_map arguments;
    po::store(po::command_line_parser(argc, argv).
              options(commandline_options).positional(p).run(), arguments);
    po::notify(arguments);
    
    if (arguments.count("help") || !arguments.count("input"))
    {
        std::cout << "Usage: " << argv[0] << " [Options] input(s)" << std::endl;
        std::cout << commandline_options << std::endl;
        return 1;
    }
    
    StatsCollector sc;
    
    a4::io::A4Input in;
    
    foreach (std::string filename, input_files)
        in.add_file(filename);
        
    shared<a4::io::InputStream> stream = in.get_stream();
    // Stream in events we don't care about
    size_t i = 0;
    for (; i < event_index; i++)
        if (!stream->next())
            throw a4::Fatal("Ran out of events! There are only ", i, " on the file!");
            
    const size_t total = event_index + event_count;
    for (; i < total; i++) {
        a4::io::A4Message m = stream->next();
        if (!m)
            throw a4::Fatal("Ran out of events! There are only ", i, " on the file!");
        if (!collect_stats)
            dump_message(*m.message, variables);
        else
            sc.collect(*m.message);
    }
    
    if (collect_stats)
        std::cout << sc;
}
