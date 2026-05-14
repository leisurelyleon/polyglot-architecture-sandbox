import java.nio.ByteBuffer;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.file.*;
import java.util.concurrent.*;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

public class HlsTranscodingOrchestrator {
    
    private static final ExecutorService TRANSCODE_POOL = Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors());
    private static final String OUTPUT_DIR = "/var/media/hls_out/";

    public static void main(String[] args) throws Exception {
        System.out.println("[Media Server] Starting Adaptive Bitrate Transcode Job...");
        
        // Simulating a video broken into 100 raw chunks
        List<Integer> chunkSequence = IntStream.range(0, 100).boxed().collect(Collectors.toList());

        // Process all 100 chunks concurrently for 1080p and 720p profiles
        CompletableFuture<Void> profile1080p = processProfile(chunkSequence, "1080p", 5000);
        CompletableFuture<Void> profile720p  = processProfile(chunkSequence, "720p", 2500);

        // Wait for both entire profiles to finish
        CompletableFuture.allOf(profile1080p, profile720p).join();
        
        System.out.println("[Media Server] Master Playlist Generated successfully.");
        TRANSCODE_POOL.shutdown();
    }

    private static CompletableFuture<Void> processProfile(List<Integer> chunks, String resolution, int bitrateKbps) {
        // Map each chunk to a non-blocking asynchronous transcode task
        List<CompletableFuture<String>> segmentFutures = chunks.stream()
            .map(chunkId -> CompletableFuture.supplyAsync(() -> executeFFmpeg(chunkId, resolution, bitrateKbps), TRANSCODE_POOL))
            .collect(Collectors.toList());

        // When all segments for this profile are done, write the m3u8 manifest
        return CompletableFuture.allOf(segmentFutures.toArray(new CompletableFuture[0]))
            .thenRunAsync(() -> {
                try {
                    writeManifest(resolution, segmentFutures);
                } catch (Exception e) {
                    throw new CompletionException(e);
                }
            });
    }

    private static String executeFFmpeg(int chunkId, String resolution, int bitrate) {
        // In reality, this would execute a ProcessBuilder to call raw FFmpeg/x264
        try {
            Thread.sleep(ThreadLocalRandom.current().nextInt(50, 150)); // Simulate CPU bound work
        } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        
        return String.format("segment_%s_%03d.ts", resolution, chunkId);
    }

    private static void writeManifest(String resolution, List<CompletableFuture<String>> completedSegments) throws Exception {
        Path manifestPath = Paths.get(OUTPUT_DIR, resolution + "_playlist.m3u8");
        
        StringBuilder manifest = new StringBuilder();
        manifest.append("#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:2\n");

        for (CompletableFuture<String> future : completedSegments) {
            manifest.append("#EXTINF:2.000,\n").append(future.join()).append("\n");
        }
        manifest.append("#EXT-X-ENDLIST\n");

        // Java NIO Asynchronous File Writing
        AsynchronousFileChannel fileChannel = AsynchronousFileChannel.open(
            manifestPath, StandardOpenOption.CREATE, StandardOpenOption.WRITE);
            
        ByteBuffer buffer = ByteBuffer.wrap(manifest.toString().getBytes());
        Future<Integer> operation = fileChannel.write(buffer, 0);
        
        operation.get(); // Await write completion
        fileChannel.close();
        System.out.println("[Manifest Builder] Wrote " + resolution + " playlist to disk.");
    }
}
