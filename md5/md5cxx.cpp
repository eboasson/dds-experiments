#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>

#include <unistd.h>

#include "dds/dds.hpp"
#include "md5_type.hpp"

using namespace org::eclipse::cyclonedds;

static const char *saved_argv0;

static const std::array<MD5, 2> data{
  MD5{{
    0xd1,0x31,0xdd,0x02,0xc5,0xe6,0xee,0xc4,0x69,0x3d,0x9a,0x06,0x98,0xaf,0xf9,0x5c,
    0x2f,0xca,0xb5,0x87,0x12,0x46,0x7e,0xab,0x40,0x04,0x58,0x3e,0xb8,0xfb,0x7f,0x89,
    0x55,0xad,0x34,0x06,0x09,0xf4,0xb3,0x02,0x83,0xe4,0x88,0x83,0x25,0x71,0x41,0x5a,
    0x08,0x51,0x25,0xe8,0xf7,0xcd,0xc9,0x9f,0xd9,0x1d,0xbd,0xf2,0x80,0x37,0x3c,0x5b,
    0xd8,0x82,0x3e,0x31,0x56,0x34,0x8f,0x5b,0xae,0x6d,0xac,0xd4,0x36,0xc9,0x19,0xc6,
    0xdd,0x53,0xe2,0xb4,0x87,0xda,0x03,0xfd,0x02,0x39,0x63,0x06,0xd2,0x48,0xcd,0xa0,
    0xe9,0x9f,0x33,0x42,0x0f,0x57,0x7e,0xe8,0xce,0x54,0xb6,0x70,0x80,0xa8,0x0d,0x1e,
    0xc6,0x98,0x21,0xbc,0xb6,0xa8,0x83,0x93,0x96,0xf9,0x65,0x2b,0x6f,0xf7,0x2a,0x70 },
    0, 0},
  MD5{{
    0xd1,0x31,0xdd,0x02,0xc5,0xe6,0xee,0xc4,0x69,0x3d,0x9a,0x06,0x98,0xaf,0xf9,0x5c,
    0x2f,0xca,0xb5,0x07,0x12,0x46,0x7e,0xab,0x40,0x04,0x58,0x3e,0xb8,0xfb,0x7f,0x89,
    0x55,0xad,0x34,0x06,0x09,0xf4,0xb3,0x02,0x83,0xe4,0x88,0x83,0x25,0xf1,0x41,0x5a,
    0x08,0x51,0x25,0xe8,0xf7,0xcd,0xc9,0x9f,0xd9,0x1d,0xbd,0x72,0x80,0x37,0x3c,0x5b,
    0xd8,0x82,0x3e,0x31,0x56,0x34,0x8f,0x5b,0xae,0x6d,0xac,0xd4,0x36,0xc9,0x19,0xc6,
    0xdd,0x53,0xe2,0x34,0x87,0xda,0x03,0xfd,0x02,0x39,0x63,0x06,0xd2,0x48,0xcd,0xa0,
    0xe9,0x9f,0x33,0x42,0x0f,0x57,0x7e,0xe8,0xce,0x54,0xb6,0x70,0x80,0x28,0x0d,0x1e,
    0xc6,0x98,0x21,0xbc,0xb6,0xa8,0x83,0x93,0x96,0xf9,0x65,0xab,0x6f,0xf7,0x2a,0x70 },
    0, 0}
};

static const std::vector<size_t> make_delta()
{
  std::vector<size_t> delta{};
  for (size_t i = 0; i < data[0].k().size(); i++)
    if (data[0].k()[i] != data[1].k()[i])
      delta.push_back(i);
  return delta;
}
static const auto delta = make_delta();

static void usage()
{
  std::cout <<
    "usage: " << std::string(saved_argv0) <<
    " sub | pub {A|B} {0|1} {0|1|write|dispose|unregister}..." << std::endl;
  std::exit(1);
}

static const std::string istr(dds::sub::status::InstanceState s)
{
  if (s == dds::sub::status::InstanceState::alive())
    return "A";
  else if (s == dds::sub::status::InstanceState::not_alive_disposed())
    return "D";
  else // not_alive_no_writers => "unregistered"
    return "U";
}

static const std::string vstr(dds::sub::status::ViewState s)
{
  if (s == dds::sub::status::ViewState::new_view())
    return "N";
  else // "not_new" = "old", right?
    return "O";
}

static const std::string sstr(dds::sub::status::SampleState s)
{
  if (s == dds::sub::status::SampleState::read())
    return "S"; // stale
  else
    return "F"; // fresh
}

static const std::string instkeystr(const MD5& x)
{
  std::stringstream s;
  s << std::setfill('0') << std::setw(2) << std::hex;
  for (size_t i = 0; i < delta.size(); i++)
    s << static_cast<unsigned>(x.k()[delta[i]]);
  return s.str() + "...";
}

static void print(const dds::sub::SampleInfo& si, const MD5& x)
{
  std::cout
    << "  is/vs/ss "
    << istr(si.state().instance_state()) << "/" << vstr(si.state().view_state()) << "/" << sstr(si.state().sample_state())
    << " ph " << si.publication_handle()->handle()
    << " ih " << si.instance_handle()->handle()
    << " key " << instkeystr(x);
  if (si.valid())
    std::cout << " pubid " << x.pubid() << " seq " << x.seq();
  std::cout << std::endl;
}

static void dosub(const dds::domain::DomainParticipant dp, const dds::topic::Topic<MD5> tp)
{
  dds::sub::Subscriber sub{dp};
  dds::sub::DataReader<MD5> rd{sub, tp, tp.qos()};
  dds::sub::cond::ReadCondition rdcond{rd,
    dds::sub::status::DataState(dds::sub::status::SampleState::not_read(),
                                dds::sub::status::ViewState::any(),
                                dds::sub::status::InstanceState::any())};
  dds::core::cond::WaitSet ws;
  ws += rdcond;
  std::cout << "Waiting for data ..." << std::endl;
  while (true)
  {
    ws.wait(dds::core::Duration::infinite());
    
    std::cout << std::chrono::high_resolution_clock::now().time_since_epoch() << " RHC now:" << std::endl;
    auto xs = rd.read();
    std::for_each(xs.begin(), xs.end(), [](const auto& x) { print(x.info(), x.data()); });
  }
}

static void dopub(const dds::domain::DomainParticipant dp, const dds::topic::Topic<MD5> tp, std::deque<std::string> args)
{
  if (!((args[0] == "A" || args[0] == "B") && (args[1] == "0" || args[1] == "1")))
    usage();
  const char pubid = args[0][0];
  args.pop_front();
  int seq = 0;
  size_t inst = std::stoul(args[0]);
  args.pop_front();

  dds::pub::Publisher pub{dp};
  dds::pub::DataWriter<MD5> wr{pub, tp, tp.qos()};

  std::this_thread::sleep_for(std::chrono::seconds(2));
  while (!args.empty())
  {
    const auto cmd = args[0];
    args.pop_front();
    std::cout << std::chrono::high_resolution_clock::now().time_since_epoch() << " pub " << pubid << " seq " << seq << " inst " << instkeystr(data[inst]) << ": " << cmd << std::endl;
    if (cmd == "0") {
      inst = 0;
    } else if (cmd == "1") {
      inst = 1;
    } else if (cmd == "write") {
      wr.write(MD5{data[inst].k(), pubid, seq++});
    } else if (cmd == "dispose")
      wr.dispose_instance(MD5{data[inst].k(), pubid, seq++});
    else if (cmd == "unregister")
      wr.unregister_instance(MD5{data[inst].k(), pubid, seq++});
    else
    {
      std::cout << "invalid operation" << std::endl;
      std::exit(1);
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  std::this_thread::sleep_for(std::chrono::seconds(5));
  std::cout << std::chrono::high_resolution_clock::now().time_since_epoch() << " pub done" << std::endl;
}

int main (int argc, char **argv)
{
  saved_argv0 = argv[0];
  std::deque<std::string> args{argv + 1, argv + argc};
  if (args.empty())
    usage();

  dds::domain::DomainParticipant dp{0};
  auto tpqos = dp.default_topic_qos()
    << dds::core::policy::Reliability::Reliable(dds::core::Duration::infinite())
    << dds::core::policy::Durability::TransientLocal();
  dds::topic::Topic<MD5> tp(dp, "MD5", tpqos);

  const auto cmd = args[0];
  args.pop_front();
  if (cmd == "sub")
    dosub(dp, tp);
  else
    dopub(dp, tp, args);
  return 0;
}
