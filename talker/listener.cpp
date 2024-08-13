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
#include "data.hpp"

using namespace org::eclipse::cyclonedds;

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

template<typename T>
static void print(const dds::sub::SampleInfo& si, const T& x)
{
  std::cout
    << "  is/vs/ss "
    << istr(si.state().instance_state()) << "/" << vstr(si.state().view_state()) << "/" << sstr(si.state().sample_state())
    << " ph " << si.publication_handle()->handle()
    << " ih " << si.instance_handle()->handle()
    << " " << x;
  std::cout << std::endl;
}

template<typename T>
static int doit ()
{
  dds::domain::DomainParticipant dp{0};
  auto tpqos = dp.default_topic_qos()
    << dds::core::policy::Reliability::Reliable(dds::core::Duration::infinite())
    << dds::core::policy::Durability::Volatile();
  dds::topic::Topic<T> tp(dp, "Data", tpqos);
  dds::sub::Subscriber sub{dp};
  dds::sub::DataReader<T> rd{sub, tp, tp.qos()};
  dds::sub::cond::ReadCondition rdcond{rd,
    dds::sub::status::DataState(dds::sub::status::SampleState::not_read(),
                                dds::sub::status::ViewState::any(),
                                dds::sub::status::InstanceState::any())};
  dds::core::cond::WaitSet ws;
  ws += rdcond;
  while (true)
  {
    ws.wait(dds::core::Duration::infinite());
    std::cout << std::chrono::high_resolution_clock::now().time_since_epoch() << " RHC now:" << std::endl;
    auto xs = rd.read();
    std::for_each(xs.begin(), xs.end(), [](const auto& x) { print(x.info(), x.data()); });
  }
  return 0;
}

int main (int argc, char **argv)
{
  if (argc != 2) {
    std::cout << "usage: " << std::string(argv[0]) << " TOPIC" << std::endl;
    return 1;
  }
  const std::string topic = std::string(argv[1]);
  if (topic != "I" && topic != "S" && topic != "IL" && topic != "ILS") {
    std::cout << "invalid TOPIC, should be I, S, IL or ILS" << std::endl;
    return 1;
  }
  if (topic == "I")
    return doit<DataI>();
  else if (topic == "S")
    doit<DataS>();
  else if (topic == "IL")
    return doit<DataIL>();
  else if (topic == "ILS")
    return doit<DataILS>();
  throw std::runtime_error("oopsie: unhandled topic " + topic);
}
