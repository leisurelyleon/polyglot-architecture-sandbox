import { createBuilder } from 'xmlbuilder2';

// 1. Domain Modeling of the Video Representations
interface VideoRepresentation {
    id: string;
    width: number;
    height: number;
    bandwidthKbps: number;
    codecs: string; // e.g., 'avc1.640028' (H.264) or 'hev1.1.6.L93.B0' (H.265)
    frameRate: string;
}

export class DashManifestGenerator {
    private readonly durationSeconds: number;
    private readonly segmentDurationSeconds: number;
    private readonly representations: VideoRepresentation[];

    constructor(durationSeconds: number, segmentDurationSeconds: number, representations: VideoRepresentation[]) {
        this.durationSeconds = durationSeconds;
        this.segmentDurationSeconds = segmentDurationSeconds;
        this.representations = representations;
    }

    // 2. The Core Manifest Builder
    public generateMPD(): string {
        // Convert total duration into the ISO 8601 format required by MPEG-DASH
        const mediaDurationIso = `PT${this.durationSeconds}S`;

        // Initialize the root of the XML Document
        const doc = createBuilder({ version: '1.0', encoding: 'UTF-8' })
            .ele('MPD', {
                xmlns: 'urn:mpeg:dash:schema:mpd:2011',
                profiles: 'urn:mpeg:dash:profile:isoff-live:2011',
                type: 'static',
                mediaPresentationDuration: mediaDurationIso,
                minBufferTime: 'PT1.5S'
            });

        const period = doc.ele('Period', { id: '0', start: 'PT0.0S' });

        // 3. Create the Adaptation Set (Groups different qualities of the SAME video)
        const adaptationSet = period.ele('AdaptationSet', {
            id: '1',
            contentType: 'video',
            segmentAlignment: 'true',
            bitstreamSwitching: 'true',
            maxWidth: Math.max(...this.representations.map(r => r.width)),
            maxHeight: Math.max(...this.representations.map(r => r.height)),
            maxFrameRate: this.representations[0].frameRate
        });

        // 4. Implement the Segment Template
        // This math equation replaces the need to list thousands of segment URLs manually.
        // It tells the player: "To get chunk 5 of 1080p, fetch 'video_1080p_5.m4s'"
        adaptationSet.ele('SegmentTemplate', {
            timescale: '1000',
            duration: (this.segmentDurationSeconds * 1000).toString(),
            initialization: 'video_$RepresentationID$_init.mp4',
            media: 'video_$RepresentationID$_$Number$.m4s',
            startNumber: '1'
        });

        // 5. Inject the varying Representations (1080p, 720p, etc.)
        for (const rep of this.representations) {
            adaptationSet.ele('Representation', {
                id: rep.id,
                mimeType: 'video/mp4',
                codecs: rep.codecs,
                width: rep.width.toString(),
                height: rep.height.toString(),
                frameRate: rep.frameRate,
                bandwidth: (rep.bandwidthKbps * 1000).toString()
            });
        }

        // Return the heavily formatted XML string ready to be served over HTTP
        return doc.end({ prettyPrint: true });
    }
}

// --- Execution Example ---
const generator = new DashManifestGenerator(3600, 4, [
    { id: '1080p', width: 1920, height: 1080, bandwidthKbps: 6000, codecs: 'avc1.640028', frameRate: '60/1' },
    { id: '720p', width: 1280, height: 720, bandwidthKbps: 3000, codecs: 'avc1.4d401f', frameRate: '60/1' },
    { id: '480p', width: 854, height: 480, bandwidthKbps: 1000, codecs: 'avc1.42c01e', frameRate: '30/1' }
]);

console.log(generator.generateMPD());
