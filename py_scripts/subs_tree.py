#!/usr/bin/python
import dps
import time

#  Builds this subscription tree
#
#   a/b/c ----\                                   /---- 1/2/3 <----- +/#
#              \                                 /
#   d/e/f ------> A/A -------> B/B <------- C/C <------ 4/5/6
#              /                ^                \
#   g/h/i ----/                 |                 \---- 7/8/9
#                               |
#                              +/#

nodes = {}
subs = {}

key_id = [
    0xed,0x54,0x14,0xa8,0x5c,0x4d,0x4d,0x15,0xb6,0x9f,0x0e,0x99,0x8a,0xb1,0x71,0xf2
]
network_key_id = [
    0x4c,0xfc,0x6b,0x75,0x0f,0x80,0x95,0xb3,0x6c,0xb7,0xc1,0x2f,0x65,0x2d,0x38,0x26
]
key_data = [
    0xf6,0xeb,0xcb,0xa4,0x25,0xdb,0x3b,0x7e,0x73,0x03,0xe6,0x9c,0x60,0x35,0xae,0x11,
    0xae,0x40,0x0b,0x84,0xf0,0x03,0xcc,0xf9,0xce,0x5c,0x5f,0xd0,0xae,0x51,0x0a,0xcc
]
network_key = [
    0x11,0x21,0xbb,0xf4,0x9f,0x5e,0xe5,0x5a,0x11,0x86,0x47,0xe6,0x3d,0xc6,0x59,0xa4,
    0xc3,0x1f,0x16,0x56,0x7f,0x1f,0xb8,0x4d,0xe1,0x09,0x28,0x26,0xd5,0xc0,0xf1,0x34
]
key_store = dps.create_memory_key_store()
dps.set_content_key(key_store, key_id, key_data)
dps.set_network_key(key_store, network_key_id, network_key)

def on_pub(sub, pub, payload):
    print "Received on port %d" % dps.get_port_number(dps.subscription_get_node(sub))
    print "Pub %s(%d) matches:" % (dps.publication_get_uuid(pub), dps.publication_get_sequence_num(pub))
    print "  pub " + " | ".join(dps.publication_get_topics(pub))
    print "  sub " + " | ".join(dps.subscription_get_topics(sub))
    print payload

def subscriber(port, topic, connect_port):
    nodes[port] = dps.create_node("/", key_store, None)
    dps.start_node(nodes[port], 0, port)
    subs[port] = dps.create_subscription(nodes[port], [topic])
    dps.subscribe(subs[port], on_pub)
    if (connect_port != 0):
        addr = dps.create_address()
        ret = dps.link_to(nodes[port], None, connect_port, addr)
        if (ret == dps.OK):
            print "Linked %d to %d" % (port, connect_port)
        dps.destroy_address(addr)

# Enable or disable (default) DPS debug output
dps.cvar.debug = False

subscriber(20000, 'B/B', 0)
subscriber(30000, 'A/A', 20000)
subscriber(50000, 'C/C', 20000)

subscriber(40000, 'a/b/c', 30000)
subscriber(40001, 'd/e/f', 30000)
subscriber(40002, 'g/h/i', 30000)

subscriber(60000, '1/2/3', 50000)
subscriber(60001, '4/5/6', 50000)
subscriber(60002, '7/8/9', 50000)

time.sleep(15)
subscriber(0, '+/#', 60000)

time.sleep(15)
subscriber(0, '+/#', 20000)
