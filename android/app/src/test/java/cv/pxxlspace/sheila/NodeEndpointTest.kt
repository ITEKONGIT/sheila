package cv.pxxlspace.sheila

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class NodeEndpointTest {
    @Test
    fun canonicalEndpointNeverDisplaysAPort() {
        assertEquals("http://sheila.local", NodeEndpoint.Canonical.url)
        assertFalse(NodeEndpoint.Canonical.url.substringAfter("//").contains(":"))
    }

    @Test
    fun ipFallbackNeverDisplaysAPort() {
        val endpoint = NodeEndpoint.IpFallback("192.168.43.1", "mdns")
        assertEquals("http://192.168.43.1", endpoint.url)
        assertFalse(endpoint.url.substringAfter("//").contains(":"))
    }
}
