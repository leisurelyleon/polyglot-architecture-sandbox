import org.apache.kafka.clients.consumer.*;
import org.apache.kafka.clients.producer.*;
import org.apache.kafka.common.TopicPartition;
import org.apache.kafka.common.errors.ProducerFencedException;

import java.time.Duration;
import java.util.*;

public class ExactlyOnceStreamProcessor {
    private static final String CONSUMER_GROUP_ID = "finance-analytics-group-v1";
    private static final String SOURCE_TOPIC = "raw-transactions";
    private static final String DEST_TOPIC = "aggregated-metrics";

    public static void main(String[] args) {
        // 1. Configure the Consumer (Must only read fully committed data)
        Properties consumerProps = new Properties();
        consumerProps.put(ConsumerConfig.BOOTSTRAP_SERVERS_CONFIG, "broker1:9092,broker2:9092");
        consumerProps.put(ConsumerConfig.GROUP_ID_CONFIG, CONSUMER_GROUP_ID);
        consumerProps.put(ConsumerConfig.ENABLE_AUTO_COMMIT_CONFIG, "false");
        consumerProps.put(ConsumerConfig.ISOLATION_LEVEL_CONFIG, "read_committed");

        KafkaConsumer<String, String> consumer = new KafkaConsumer<>(consumerProps);
        consumer.subscribe(Collections.singletonList(SOURCE_TOPIC));

        // 2. Configure the Transactional Producer (Requires a strict Transactional ID)
        Properties producerProps = new Properties();
        producerProps.put(ProducerConfig.BOOTSTRAP_SERVERS_CONFIG, "broker1:9092,broker2:9092");
        producerProps.put(ProducerConfig.TRANSACTIONAL_ID_CONFIG, "tx-finance-aggregator-001");
        producerProps.put(ProducerConfig.ENABLE_IDEMPOTENCE_CONFIG, "true");
        producerProps.put(ProducerConfig.ACKS_CONFIG, "all");

        KafkaProducer<String, String> producer = new KafkaProducer<>(producerProps);
        
        // 3. Initialize the distributed transaction state on the Kafka cluster
        producer.initTransactions();

        try {
            while (true) {
                // Poll for new events flowing through the river
                ConsumerRecords<String, String> records = consumer.poll(Duration.ofMillis(500));
                if (records.isEmpty()) continue;

                producer.beginTransaction();

                try {
                    Map<TopicPartition, OffsetAndMetadata> offsetsToCommit = new HashMap<>();

                    for (ConsumerRecord<String, String> record : records) {
                        // --- Complex Business Logic Happens Here ---
                        String enrichedData = processFinancialPayload(record.value());
                        
                        // Emit the new data to the downstream topic
                        producer.send(new ProducerRecord<>(DEST_TOPIC, record.key(), enrichedData));

                        // Track the exact offset we are currently processing
                        TopicPartition partition = new TopicPartition(record.topic(), record.partition());
                        offsetsToCommit.put(partition, new OffsetAndMetadata(record.offset() + 1));
                    }

                    // 4. Atomically commit the offsets AND the produced records
                    producer.sendOffsetsToTransaction(offsetsToCommit, CONSUMER_GROUP_ID);
                    producer.commitTransaction();
                    
                    System.out.printf("Successfully processed transaction batch of %d records.%n", records.count());

                } catch (ProducerFencedException e) {
                    // FATAL: A split-brain occurred and another instance took over our Transactional ID
                    System.err.println("Zombie process detected. Fenced out by cluster. Shutting down.");
                    producer.close();
                    System.exit(1);
                } catch (Exception e) {
                    // Transient error: Abort the transaction, Kafka will rewind the consumer offset automatically
                    producer.abortTransaction();
                    System.err.println("Transaction aborted due to processing error: " + e.getMessage());
                }
            }
        } finally {
            consumer.close();
            producer.close();
        }
    }

    private static String processFinancialPayload(String raw) {
        // Mock heavy JSON parsing and transformation
        return raw.toUpperCase() + "_PROCESSED";
    }
}
