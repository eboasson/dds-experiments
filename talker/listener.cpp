#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>

#include "dds/dds.hpp"
#include "data.hpp"


template <class T>
class ListenerTopic : public dds::topic::TopicListener<T>
{
public:
    void on_inconsistent_topic(
        dds::topic::Topic<T>&, dds::core::status::InconsistentTopicStatus const& status) override
    {

        std::cout << "Inconsistent Topic ("
                  << dds::topic::topic_type_name<T>::value()
                  << "): count=" << status.total_count()
                  << "(" << std::showpos << status.total_count_change() << ")" << std::endl;
    }
};

template <class T>
class ListenerRead : public dds::sub::DataReaderListener<T>
{
public:
    ListenerRead(){}

    void on_data_available(dds::sub::DataReader<T>&) override
    {
      std::cout << "on_data_available" << std::endl;
    }

    void on_liveliness_changed(
        dds::sub::DataReader<T>&, const dds::core::status::LivelinessChangedStatus& status) override
    {

        std::cout << "liveliness changed ("
                  << dds::topic::topic_type_name<T>::value()
                  << "): alive: " << status.alive_count()
                  << ", not alive: " << status.not_alive_count() << std::endl;
    }

    void on_requested_deadline_missed(
        dds::sub::DataReader<T>&, const dds::core::status::RequestedDeadlineMissedStatus&) override
    {

        std::cout << "on_requested_deadline_missed" << std::endl;
    }

    void on_requested_incompatible_qos(
        dds::sub::DataReader<T>& /*reader*/,
        const dds::core::status::RequestedIncompatibleQosStatus& status) override
    {
    
        std::cout << "Reader: Incompatible QoS requested: ("
                    << std::to_string(status.last_policy_id())
                    << "): " << ", "
                    << dds::topic::topic_type_name<T>::value()
                    << ", count=" << status.total_count()
                    << "("
                    << std::showpos << status.total_count_change() << ")" << std::endl;
    }

    void on_sample_lost(dds::sub::DataReader<T>&, const dds::core::status::SampleLostStatus&) override
    {
        std::cout << "on_sample_lost" << std::endl;
    }

    void on_sample_rejected(dds::sub::DataReader<T>&, const dds::core::status::SampleRejectedStatus&) override
    {
        std::cout << "on_sample_rejected" << std::endl;
    }

    void on_subscription_matched(dds::sub::DataReader<T>&, const dds::core::status::SubscriptionMatchedStatus& status) override
    {

        std::cout << "Subscription matched ("
                  << dds::topic::topic_type_name<T>::value() << "): "
                  << status.current_count() << std::endl;
    }
};

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
#ifdef DDS_BACKEND_CONNEXT
    << " ph " << si.publication_handle()
    << " ih " << si.instance_handle()
#else
    << " ph " << si.publication_handle()->handle()
    << " ih " << si.instance_handle()->handle()
#endif
    << " " << x;
  std::cout << std::endl;
}

template<typename T>
static int doit ()
{
  // Participant
  dds::domain::DomainParticipant dp{0};

  // Topic
  std::vector<dds::core::policy::DataRepresentationId> reprs;
#ifdef DDS_BACKEND_CONNEXT
    reprs.push_back(dds::core::policy::DataRepresentation::xcdr());
    reprs.push_back(dds::core::policy::DataRepresentation::xcdr2());
#else
    reprs.push_back(dds::core::policy::DataRepresentationId::XCDR1);
    reprs.push_back(dds::core::policy::DataRepresentationId::XCDR2);
#endif
  ListenerTopic<T> listenerTopic;
  auto tpqos = dp.default_topic_qos()
    << dds::core::policy::Reliability::Reliable(dds::core::Duration::infinite())
    << dds::core::policy::Durability::Volatile()
    << dds::core::policy::History::KeepLast(5)
    << dds::core::policy::DataRepresentation(reprs);
  dds::topic::Topic<T> tp(dp, "Data", tpqos, &listenerTopic, dds::core::status::StatusMask::all());

  // Subscriber
  dds::sub::Subscriber sub{dp};

  // Reader
  ListenerRead<T> listener;
  dds::sub::qos::DataReaderQos rdqos = sub.default_datareader_qos();
  rdqos = tpqos;
  dds::sub::DataReader<T> rd{sub, tp, rdqos, &listener, dds::core::status::StatusMask::all()};
  dds::sub::cond::ReadCondition rdcond{rd,
    dds::sub::status::DataState(dds::sub::status::SampleState::not_read(),
                                dds::sub::status::ViewState::any(),
                                dds::sub::status::InstanceState::any())};
  dds::core::cond::WaitSet ws;
  ws += rdcond;
  while (true)
  {
    ws.wait(dds::core::Duration::infinite());
    std::cout << (std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::milliseconds(1)) << " RHC now:" << std::endl;
    auto xs = rd.read();
    std::for_each(xs.begin(), xs.end(), [](const auto& x) {

      auto& sampleInfo = x.info();
      if (sampleInfo.valid())
      {
        print(sampleInfo, x.data());
      }
      else
      {
        std::cout << "SampleInfo is not valid, skip printing ..." << std::endl;
      }
    });
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
