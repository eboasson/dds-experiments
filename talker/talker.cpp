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

template<typename T, typename K>
static void setkey (T& d, K k) {
  d.k(k);
}

static void setkey (DataIL& d, int32_t k) {
  d.k(k);
  d.l(0x123456789abcdef0ll);
}

static void setkey (DataILS& d, int32_t k) {
  d.k(k);
  d.l(0x123456789abcdef0ll);
  d.m("doremifasollati");
}

template<typename T, typename K>
static int doit (K k, int32_t r)
{

  dds::domain::DomainParticipant dp{0};
  auto tpqos = dp.default_topic_qos()
    << dds::core::policy::Reliability::Reliable(dds::core::Duration::infinite())
    << dds::core::policy::Durability::Volatile();
  dds::topic::Topic<T> tp(dp, "Data", tpqos);
  dds::pub::Publisher pub{dp};
  dds::pub::qos::DataWriterQos wrqos = tp.qos();
  std::vector<dds::core::policy::DataRepresentationId> reprs;
  if (r == 1)
    reprs.push_back(dds::core::policy::DataRepresentationId::XCDR1);
  else
    reprs.push_back(dds::core::policy::DataRepresentationId::XCDR2);
  wrqos
    << dds::core::policy::WriterDataLifecycle::ManuallyDisposeUnregisteredInstances()
    << dds::core::policy::DataRepresentation(reprs);
  dds::pub::DataWriter<T> wr{pub, tp, wrqos};

  int32_t v = 1;
  while (true)
  {
    T d;
    setkey(d, k);
    d.v(v++);
    if ((v % 5) == 0)
      wr.dispose_instance(d);
    else
      wr.write(d);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}

int main (int argc, char **argv)
{
  if (argc != 4) {
    std::cout << "usage: " << std::string(argv[0]) << " TOPIC KEY REPR" << std::endl;
    return 1;
  }
  const std::string topic = std::string(argv[1]);
  if (topic != "I" && topic != "S" && topic != "IL" && topic != "ILS") {
    std::cout << "invalid TOPIC, should be I, S, IL or ILS" << std::endl;
    return 1;
  }
  const std::string kstr = std::string(argv[2]);
  const int32_t r = std::atoi(argv[3]);
  if (r < 1 || r > 2) {
    std::cout << "invalid REPR, should be 1 (for XCDR) or 2 (for XCDR2)" << std::endl;
    return 1;
  }
  if (topic == "I")
    return doit<DataI, int32_t>(std::stoi(kstr), r);
  else if (topic == "S")
    doit<DataS, std::string>(kstr, r);
  else if (topic == "IL")
    return doit<DataIL, int32_t>(std::stoi(kstr), r);
  else if (topic == "ILS")
    return doit<DataILS, std::int32_t>(std::stoi(kstr), r);
  throw std::runtime_error("oopsie: unhandled topic " + topic);
}
