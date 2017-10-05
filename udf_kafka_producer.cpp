#include <iostream>
#include <mysql.h>

extern "C"
{
    my_bool kafkaproducer(UDF_INIT *, UDF_ARGS *args, char *message);
    double kafkaproducer(UDF_INIT *initid, UDF_ARGS *args, char *is_null,char *error);
}


my_bool kafkaproducer_init(UDF_INIT *, UDF_ARGS *args, char *message)
{
    if (args->arg_count < 1)
    {
        strcpy(message,"arguments required: brokers, topic, json avro schema=");
        return 1;
    }

    /*
     * Create Avro Seralizer
     *
     * Create serdes configuration object.
     * Configuration passed through -X prop=val will be set on this object,
     * which is later passed to the serdes handle creator. */
    Serdes::Conf *sconf = Serdes::Conf::create();

    /*
     * Create configuration objects
     */
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

    /*
     * Create producer using accumulated global configuration.
     */
    RdKafka::Producer *producer = RdKafka::Producer::create(conf, errstr);
    if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        exit(1);
    }

    /*
     * Create topic handle.
     */
    RdKafka::Topic *topic = RdKafka::Topic::create(producer, topic_str,
                                                   tconf, errstr);
    if (!topic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
}

long long kafkaproducer(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error)
{
    /*
     * Seralize to avro
     */

    /*
     * Produce message
     */
    RdKafka::ErrorCode resp =
            producer->produce(topic, partition,
                              RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                              const_cast<char *>(line.c_str()), line.size(),
                              NULL, NULL);
    if (resp != RdKafka::ERR_NO_ERROR)
        std::cerr << "% Produce failed: " <<
                  RdKafka::err2str(resp) << std::endl;
    else
        std::cerr << "% Produced message (" << line.size() << " bytes)" <<
                  std::endl;

    producer->poll(0);

}

void udf_kafkaproducer_deinit()
{
    while (producer->outq_len() > 0) {
        std::cerr << "Waiting for " << producer->outq_len() << std::endl;
        producer->poll(1000);
    }
    delete topic;
    delete producer;
}

/**
 * Convert JSON to Avro Datum.
 *
 * Returns 0 on success or -1 on failure.
 */
static int json2avro (Serdes::Schema *schema, const std::string &json,
                      avro::GenericDatum **datump) {

    avro::ValidSchema *avro_schema = schema->object();

    /* Input stream from json string */
    std::istringstream iss(json);
    std::auto_ptr<avro::InputStream> json_is = avro::istreamInputStream(iss);

    /* JSON decoder */
    avro::DecoderPtr json_decoder = avro::jsonDecoder(*avro_schema);

    avro::GenericDatum *datum = new avro::GenericDatum(*avro_schema);

    try {
        /* Decode JSON to Avro datum */
        json_decoder->init(*json_is);
        avro::decode(*json_decoder, *datum);

    } catch (const avro::Exception &e) {
        std::cerr << "% JSON to Avro transformation failed: "
                  << e.what() << std::endl;
        return -1;
    }

    *datump = datum;

    return 0;
}