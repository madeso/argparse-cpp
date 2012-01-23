#include <iostream>

#include <string>
#include <sstream>
#include <boost/function.hpp>
#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#define ConverterFunction(V) boost::function<V (const std::string&)>
#define CombinerFunction(T,V) boost::function<void (T& t, const V&)>

namespace argparse
{
	class ParserError : public std::runtime_error
	{
	public:
		ParserError(const std::string& error)
			: runtime_error("error: " + error)
		{
		}
	};

	class Arguments
	{
	public:
		Arguments(int argc, char* argv[])
		{
			for(int i=1; i<argc; ++i)
			{
				args.push_back(argv[i]);
			}
		}

		const std::string operator[](int index) const
		{
			return args[index];
		}
		const bool empty() const
		{
			return args.empty();
		}
		const size_t size() const
		{
			return args.size();
		}
		const std::string get(const std::string& error = "no more arguments available")
		{
			if( empty() ) throw ParserError(error);
			const std::string r = args[0];
			args.erase(args.begin());
			return r;
		}
	private:
		std::vector<std::string> args;
	};

	template <typename T>
	class Convert
	{
	public:
		Convert(const std::string& name, T t)
		{
		}

		Convert& operator()(const std::string& name, T t)
		{
		}

		T operator()(const std::string& in)
		{
		}
	};

	template<typename T>
	T StandardConverter(const std::string& type)
	{
		std::istringstream ss(type);
		T t;
		if( ss >> t )
		{
			return t;
		}
		else
		{
			throw ParserError("Failed to parse " + type);
		}
	}

	class Count
	{
	public:
		enum Type
		{
			Const, MoreThanOne, Optional, None, ZeroOrMore
		};

		Count(size_t c)
			: mCount(c)
			, mType(Const)
		{
		}

		Count(Type t)
			: mCount(0)
			, mType(t)
		{
			assert(t != Const);
		}

		Type type() const
		{
			return mType;
		}
		size_t count() const
		{
			return mCount;
		}
	private:
		size_t mCount;
		Type mType;
	};

	/// basic class for passing along variables that only exist when parsing
	struct Running
	{
	public:
		Running(const std::string& aapp, std::ostream& ao)
			: app(aapp)
			, o(ao)
		{
		}

		const std::string& app;
		std::ostream& o;
	private:
		Running(const Running&);
		void operator=(const Running&);
	};

	class Argument
	{
	public:
		virtual ~Argument()
		{
		}

		virtual void parse(Running& r, Arguments& args, const std::string& argname) = 0;
	};

	typedef boost::function<void (Running& r, Arguments&, const std::string&)> ArgumentCallback;

	class FunctionArgument : public Argument
	{
	public:
		FunctionArgument(const ArgumentCallback& func)
			: function(func)
		{
		}

		void parse(Running& r, Arguments& args, const std::string& argname)
		{
			function(r, args, argname);
		}
	private:
		ArgumentCallback function;
	};

	template <typename T, typename V>
	class ArgumentT : public Argument
	{
	public:
		ArgumentT(T& t, const Count& co, CombinerFunction(T, V) com, ConverterFunction(V) c)
			: target(t)
			, count(co)
			, combine(com)
			, converter(c)
		{
		}

		virtual void parse(Running&, Arguments& args, const std::string& argname)
		{
			switch(count.type())
			{
			case Count::Const:
				for(size_t i=0; i<count.count(); ++i)
				{
					std::stringstream ss;
					ss << "argument " << argname << ": expected ";
					if( count.count() == 1 )
					{
						ss << "one argument";
					}
					else
					{
						ss << count.count() << " argument(s), " << i << " already given";
					}
					combine(target, converter(args.get(ss.str())));

					// abort on optional?
				}
				return;
			case Count::MoreThanOne:
				combine(target, converter(args.get("argument " + argname + ": expected atleast one argument")));
			case Count::ZeroOrMore:
				while( args.empty()==false && IsOptional(args[0])==false )
				{
					combine(target, converter(args.get("internal error")));
				}
				return;
			case Count::Optional:
				if( args.empty() ) return;
				if( IsOptional(args[0]) ) return;
				combine(target, converter(args.get("internal error")));
				return;
			case Count::None:
				return;
			default:
				assert(0 && "internal error, ArgumentT::parse invalid Count");
				throw "internal error, ArgumentT::parse invalid Count";
			}
		}
	private:
		T& target;
		Count count;
		CombinerFunction(T,V) combine;
		ConverterFunction(V) converter;
	};

	/// internal function.
	/// @returns true if arg is to be considered as an optional
	bool IsOptional(const std::string& arg)
	{
		if( arg.empty() ) return false; // todo: assert this?
		return arg[0] == '-';
	}


	// Utility class to provide optional arguments for the commandline arguments.
	class Extra
	{
	public:
		Extra()
			: mCount(1)
		{
		}

		/// set the extended help for the argument
		Extra& help(const std::string& h)
		{
			mHelp = h;
			return *this;
		}
		const std::string& help() const
		{
			return mHelp;
		}

		/// the number of values a argument can support
		Extra& count(const Count c)
		{
			mCount = c;
			return *this;
		}
		const Count& count() const
		{
			return mCount;
		}

		// the meta variable, used in usage display and help display for the arguments
		Extra& metavar(const std::string& metavar)
		{
			mMetavar = metavar;
			return *this;
		}
		const std::string& metavar() const
		{
			return mMetavar;
		}
	private:
		std::string mHelp;
		Count mCount;
		std::string mMetavar;
	};

	std::string Upper(const std::string& s)
	{
		std::string str = s;
		std::transform(str.begin(), str.end(), str.begin(), toupper);
		return str;
	}

	class Help
	{
	public:
		Help(const std::string& aname, const Extra& e)
			: name(aname)
			, help(e.help())
			, metavar(e.metavar())
			, count(e.count().type())
			, countcount(e.count().count())

		{
		}

		const std::string usage() const
		{
			if( IsOptional(name) )
			{
				return "[" + name + " " + metavarrep() + "]";
			}
			else
			{
				return metavarrep();
			}
		}

		const std::string metavarrep() const
		{
			switch(count)
			{
			case Count::None:
				return "";
			case Count::MoreThanOne:
				return metavarname() + " [" + metavarname() + " ...]";
			case Count::Optional:
				return "[" + metavarname() + "]";
			case Count::ZeroOrMore:
				return "[" + metavarname() + " [" + metavarname() + " ...]]";
			case Count::Const:
				{
					std::ostringstream ss;
					ss << "[";
					for(size_t i=0; i<countcount; ++i)
					{
						if( i != 0 )
						{
							ss << " ";
						}
						ss << metavarname();
					}
					ss << "]";
					return ss.str();
				}
			default:
				assert(false && "missing case");
				throw "invalid count type in " __FUNCTION__;
			}
		}

		const std::string metavarname() const
		{
			if( metavar.empty() == false)
			{
				return metavar;
			}
			else
			{
				if( IsOptional(name) )
				{
					return Upper(name.substr(1));
				}
				else
				{
					return name;
				}
			}
		}

		const std::string helpCommand() const
		{
			if( IsOptional(name) )
			{
				return name + " " + metavarrep();
			}
			else
			{
				return metavarname();
			}
		}

		const std::string& helpDescription() const
		{
			return help;
		}
	private:
		std::string name;
		std::string help;
		std::string metavar;
		Count::Type count;
		size_t countcount;
	};

	template<typename A, typename B>
	void Assign(A& a, const B& b)
	{
		a = b;
	}

	template<typename T>
	void PushBackVector(std::vector<T>& vec, const T& t)
	{
		vec.push_back(t);
	}

	/// main entry class that contains all arguments and does all the parsing.
	class Parser
	{
	public:
		enum ParseStatus
		{
			ParseFailed,
			ParseComplete
		};

		struct CallHelp
		{
			CallHelp(Parser* on)
				: parser(on)
			{
			}

			void operator()(Running& r, Arguments& args, const std::string& argname)
			{
				parser->writeHelp(r);
				exit(0);
			}

			Parser* parser;
		};

		Parser(const std::string& d, const std::string aappname="")
			: positionalIndex(0)
			, description(d)
			, appname(aappname)
		{
			addFunction("-h", CallHelp(this), Extra().count(Count::None).help("show this help message and exit"));
		}

		template<typename T>
		Parser& operator()(const std::string& name, T& var, const Extra& extra = Extra(), CombinerFunction(T,T) combiner = Assign<T,T>, ConverterFunction(T) co = StandardConverter<T>)
		{
			return add<T, T>(name, var, extra, combiner);
		}
		Parser& operator()(const std::string& name, ArgumentCallback func, const Extra& extra = Extra())
		{
			return addFunction(name, func, extra);
		}

		template<typename T, typename V>
		Parser& add(const std::string& name, T& var, const Extra& extra = Extra(), CombinerFunction(T,V) combiner = Assign<T,V>, ConverterFunction(V) co = StandardConverter<V>)
		{
			ArgumentPtr arg(new ArgumentT<T, V>(var, extra.count(), combiner, co));
			return insert(name, arg, extra);
		}

		Parser& addFunction(const std::string& name, ArgumentCallback func, const Extra& extra)
		{
			ArgumentPtr arg( new FunctionArgument(func) );
			return insert(name, arg, extra);
		}

		ParseStatus parseArgs(int argc, char* argv[], std::ostream& out=std::cout, std::ostream& error=std::cerr) const
		{
			Arguments args(argc, argv);
			const std::string app = argv[0];
			Running running(app, out);

			try
			{
				while( false == args.empty() )
				{
					if( IsOptional(args[0]) )
					{
						// optional
						const std::string arg = args.get();
						Optionals::const_iterator r = optionals.find(arg);
						if( r == optionals.end() )
						{
							throw ParserError("Unknown optional argument: " + arg); // todo: implement partial matching of arguments?
						}
						r->second->parse(running, args, arg);
					}
					else
					{
						if( positionalIndex >= positionals.size() )
						{
							throw ParserError("All positional arguments have been consumed: " + args[0]);
						}
						ArgumentPtr p = positionals[positionalIndex];
						++positionalIndex;
						p->parse(running, args, "POSITIONAL"); // todo: give better name or something
					}
				}

				if( positionalIndex != positionals.size() )
				{
					throw ParserError("too few arguments"); // todo: list a few missing arguments...
				}

				return ParseComplete;
			}
			catch(ParserError& p)
			{
				writeUsage(running);
				error << app << ": " << p.what() << std::endl << std::endl;
				return ParseFailed;
			}
		}

		void writeHelp(Running& r) const
		{
			writeUsage(r);
			r.o << std::endl << description << std::endl << std::endl;

			const std::string sep = "\t";
			const std::string ins = "  ";
		
			if( helpPositional.empty() == false )
			{
				r.o << "positional arguments: " << std::endl;
				BOOST_FOREACH(const Help& positional, helpPositional)
				{
					r.o << ins << positional.helpCommand() << sep << positional.helpDescription() << std::endl;
				}
		
				r.o << std::endl;
			}

			if( helpOptional.empty() == false )
			{
				r.o << "optional arguments: " << std::endl;
				BOOST_FOREACH(const Help& optional, helpOptional)
				{
					r.o << ins << optional.helpCommand() << sep << optional.helpDescription() << std::endl;
				}
			}

			r.o << std::endl;
		}
		void writeUsage(Running& r) const
		{
			r.o << "usage: " << r.app;
			BOOST_FOREACH(const Help& optional, helpOptional)
			{
				r.o << " " << optional.usage();
			}

			BOOST_FOREACH(const Help& positional, helpPositional)
			{
				r.o << " " << positional.usage();
			}
			r.o << std::endl;
		}
	private:
		typedef boost::shared_ptr<Argument> ArgumentPtr;

		Parser& insert(const std::string& name, ArgumentPtr arg, const Extra& extra)
		{
			if( IsOptional(name) )
			{
				optionals.insert(Optionals::value_type(name, arg));
				helpOptional.push_back(Help(name, extra));
				return *this;
			}
			else
			{
				positionals.push_back(arg);
				helpPositional.push_back(Help(name,extra));
				return *this;
			}
		}

		std::string description;
		std::string appname;

		typedef std::map<std::string, ArgumentPtr> Optionals;
		Optionals optionals;

		typedef std::vector<ArgumentPtr> Positionals;
		Positionals positionals;
		mutable size_t positionalIndex; // todo: mutable or change parseArgs to nonconst?

		std::vector<Help> helpOptional;
		std::vector<Help> helpPositional;
	};
}

// -------------------

enum MyEnum
{
	MyVal, MyVal2
};

void main(int argc, char* argv[])
{
	std::string compiler;
	int i;
	int op=2;
	std::vector<std::string> strings;
	//MyEnum v;
	bool ok = argparse::Parser::ParseComplete ==
		argparse::Parser("description")
		("compiler", compiler)
		("int", i)
		("-op", op)
		.add<std::vector<std::string>, std::string>("-strings", strings, argparse::Extra().count( argparse::Count::MoreThanOne ).metavar("string"), argparse::PushBackVector<std::string>) // todo: is this beautifiable?
		//("-enum", &v, Convert<MyEnum>("MyVal", MyEnum::MyVal)("MyVal2", MyEnum::MyVal2) )
		.parseArgs(argc, argv);
	if( ok == false ) return;
	std::cout << compiler << " " << i << " " << op << std::endl;
	BOOST_FOREACH(const std::string& s, strings)
	{
		std:: cout << s << " " << std::endl;
	}
}
