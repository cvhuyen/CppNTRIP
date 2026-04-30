// TestPahoMqtt.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <windows.h>
#include <cassert>
#include <iostream>
#include "MQTTClient.h"
struct Options
{
	char* connection;         /**< connection to system under test. */
	char** haconnections;
	char* proxy_connection;
	int hacount;
	int verbose;
	int test_no;
	int MQTTVersion;
	int iterations;
} options =
{
	(char*)"mqtt://broker.freemqtt.com:1883",
	NULL,
	NULL,
	0,
	0,
	0,
	MQTTVERSION_DEFAULT,
	1,
};
int failures = 0;
/*********************************************************************

Test1: single-threaded client

*********************************************************************/
void test1_sendAndReceive(MQTTClient c, int qos, char* test_topic)
{
	MQTTClient_deliveryToken dt;
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_message* m = NULL;
	char* topicName = NULL;
	int topicLen;
	int i = 0;
	int iterations = 50;
	int rc;

	printf("%d messages at QoS %d", iterations, qos);
	pubmsg.payload = (void*)"a much longer message that we can shorten to the extent that we need to payload up to 11";
	pubmsg.payloadlen = 11;
	pubmsg.qos = qos;
	pubmsg.retained = 0;

	for (i = 0; i < iterations; ++i)
	{
		if (i % 10 == 0)
			rc = MQTTClient_publish(c, test_topic, pubmsg.payloadlen, pubmsg.payload, pubmsg.qos, pubmsg.retained, &dt);
		else
			rc = MQTTClient_publishMessage(c, test_topic, &pubmsg, &dt);
		assert("Good rc from publish", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

		if (qos > 0)
		{
			rc = MQTTClient_waitForCompletion(c, dt, 5000L);
			assert("Good rc from waitforCompletion", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		}

		rc = MQTTClient_receive(c, &topicName, &topicLen, &m, 5000);
		assert("Good rc from receive", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
		if (topicName)
		{
			printf("Message received on topic %s is %.*s", topicName, m->payloadlen, (char*)(m->payload));
			if (pubmsg.payloadlen != m->payloadlen ||
				memcmp(m->payload, pubmsg.payload, m->payloadlen) != 0)
			{
				failures++;
				printf("Error: wrong data - received lengths %d %d", pubmsg.payloadlen, m->payloadlen);
				break;
			}
			MQTTClient_free(topicName);
			MQTTClient_freeMessage(&m);
		}
		else
			printf("No message received within timeout period\n");
	}

	/* receive any outstanding messages */
	MQTTClient_receive(c, &topicName, &topicLen, &m, 2000);
	while (topicName)
	{
		printf("Message received on topic %s is %.*s.\n", topicName, m->payloadlen, (char*)(m->payload));
		MQTTClient_free(topicName);
		MQTTClient_freeMessage(&m);
		MQTTClient_receive(c, &topicName, &topicLen, &m, 2000);
	}
}
int test1(struct Options options)
{
	int subsqos = 2;
	MQTTClient c;
	MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
	MQTTClient_willOptions wopts = MQTTClient_willOptions_initializer;
	int rc = 0;
	char* test_topic = (char*)"C client test1";

	
	failures = 0;
	printf("Starting test 1 - single threaded client using receive");

	rc = MQTTClient_create(&c, options.connection, "single_threaded_test",
		MQTTCLIENT_PERSISTENCE_DEFAULT, NULL);
	assert("good rc from create", rc == MQTTCLIENT_SUCCESS, "rc was %d\n", rc);
	if (rc != MQTTCLIENT_SUCCESS)
	{
		MQTTClient_destroy(&c);
		goto exit;
	}

	opts.keepAliveInterval = 20;
	opts.cleansession = 1;
	opts.username = "freemqtt";
	opts.password = "public";
	opts.MQTTVersion = options.MQTTVersion;
	if (options.haconnections != NULL)
	{
		opts.serverURIs = options.haconnections;
		opts.serverURIcount = options.hacount;
	}

	opts.will = &wopts;
	opts.will->message = "will message";
	opts.will->qos = 1;
	opts.will->retained = 0;
	opts.will->topicName = "will topic";
	opts.will = NULL;

	printf("Connecting");
	rc = MQTTClient_connect(c, &opts);
	assert("Good rc from connect", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	if (rc != MQTTCLIENT_SUCCESS)
		goto exit;

	rc = MQTTClient_subscribe(c, test_topic, subsqos);
	assert("Good rc from subscribe", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	test1_sendAndReceive(c, 0, test_topic);
	test1_sendAndReceive(c, 1, test_topic);
	test1_sendAndReceive(c, 2, test_topic);

	printf("Stopping\n");

	rc = MQTTClient_unsubscribe(c, test_topic);
	assert("Unsubscribe successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	rc = MQTTClient_disconnect(c, 0);
	assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	/* Just to make sure we can connect again */
	rc = MQTTClient_connect(c, &opts);
	assert("Connect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);
	rc = MQTTClient_disconnect(c, 0);
	assert("Disconnect successful", rc == MQTTCLIENT_SUCCESS, "rc was %d", rc);

	MQTTClient_destroy(&c);

exit:
	printf("TEST1: test %s. %d tests run, %d failures.",
		(failures == 0) ? "passed" : "failed", 1, failures);	
	return failures;
}
int main()
{
    std::cout << "Hello World!\n";	
	test1(options);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
