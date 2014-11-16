package org.jwz.xscreensaver;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

/**
 * A utility class to create buffers.
 *
 * All public methods are static. The Singleton pattern was avoided to avoid concerns about
 * threading and the Android life cycle. If needed, It can be implemented later given some research.
 */
public class BufferFactory {
    // This class cannot and should not be instantiated
    private BufferFactory() {}

    // We use Buffer.allocateDirect() to get memory outside of
    // the normal, garbage collected heap. I think this is done
    // because the buffer is subject to native I/O.
    // See http://download.oracle.com/javase/1.4.2/docs/api/java/nio/ByteBuffer.html#direct

    /**
     * Creates a buffer of floats using memory outside the normal, garbage collected heap
     *
     * @param capacity		The number of primitives to create in the buffer.
     */
    public static FloatBuffer createFloatBuffer(int capacity) {
        // 4 is the number of bytes in a float
        ByteBuffer vbb = ByteBuffer.allocateDirect(capacity * 4);
        vbb.order(ByteOrder.nativeOrder());
        return vbb.asFloatBuffer();
    }

    /**
     * Creates a buffer of shorts using memory outside the normal, garbage collected heap
     *
     * @param capacity		The number of primitives to create in the buffer.
     */
    public static ShortBuffer createShortBuffer(int capacity) {
        // 2 is the number of bytes in a short
        ByteBuffer vbb = ByteBuffer.allocateDirect(capacity * 2);
        vbb.order(ByteOrder.nativeOrder());
        return vbb.asShortBuffer();
    }
}
