import pytest

from helpers.cluster import ClickHouseCluster, is_arm

cluster = ClickHouseCluster(__file__)
node1 = cluster.add_instance("node1", main_configs=["configs/config_with_hosts.xml"])
node2 = cluster.add_instance(
    "node2", main_configs=["configs/config_with_only_primary_hosts.xml"]
)
node3 = cluster.add_instance(
    "node3", main_configs=["configs/config_with_only_regexp_hosts.xml"]
)
node4 = cluster.add_instance(
    "node4", main_configs=[]
)  # No `remote_url_allow_hosts` at all.
node5 = cluster.add_instance(
    "node5", main_configs=["configs/config_without_allowed_hosts.xml"]
)
node6 = cluster.add_instance("node6", main_configs=["configs/config_for_remote.xml"])


@pytest.fixture(scope="module")
def start_cluster():
    try:
        cluster.start()
        yield cluster
    finally:
        cluster.shutdown()


def test_config_with_hosts(start_cluster):
    assert (
        node1.query(
            "CREATE TABLE table_test_1_1 (word String) Engine=URL('http://host:80', CSV)"
        )
        == ""
    )
    assert (
        node1.query(
            "CREATE TABLE table_test_1_2 (word String) Engine=URL('https://yandex.ru', CSV)"
        )
        == ""
    )
    assert "not allowed" in node1.query_and_get_error(
        "CREATE TABLE table_test_1_4 (word String) Engine=URL('https://host:123', CSV)"
    )
    assert "not allowed" in node1.query_and_get_error(
        "CREATE TABLE table_test_1_4 (word String) Engine=URL('https://yandex2.ru', CSV)"
    )
    node1.query("DROP TABLE table_test_1_1")
    node1.query("DROP TABLE table_test_1_2")


def test_config_with_only_primary_hosts(start_cluster):
    assert (
        node2.query(
            "CREATE TABLE table_test_2_1 (word String) Engine=URL('https://host:80', CSV)"
        )
        == ""
    )
    assert (
        node2.query(
            "CREATE TABLE table_test_2_2 (word String) Engine=URL('https://host:123', CSV)"
        )
        == ""
    )
    assert (
        node2.query(
            "CREATE TABLE table_test_2_3 (word String) Engine=URL('https://yandex.ru', CSV)"
        )
        == ""
    )
    assert (
        node2.query(
            "CREATE TABLE table_test_2_4 (word String) Engine=URL('https://yandex.ru:87', CSV)"
        )
        == ""
    )
    assert "not allowed" in node2.query_and_get_error(
        "CREATE TABLE table_test_2_5 (word String) Engine=URL('https://host', CSV)"
    )
    assert "not allowed" in node2.query_and_get_error(
        "CREATE TABLE table_test_2_5 (word String) Engine=URL('https://host:234', CSV)"
    )
    assert "not allowed" in node2.query_and_get_error(
        "CREATE TABLE table_test_2_6 (word String) Engine=URL('https://yandex2.ru', CSV)"
    )

    node2.query("DROP TABLE table_test_2_1")
    node2.query("DROP TABLE table_test_2_2")
    node2.query("DROP TABLE table_test_2_3")
    node2.query("DROP TABLE table_test_2_4")


def test_config_with_only_regexp_hosts(start_cluster):
    assert (
        node3.query(
            "CREATE TABLE table_test_3_1 (word String) Engine=URL('https://host:80', CSV)"
        )
        == ""
    )
    assert (
        node3.query(
            "CREATE TABLE table_test_3_2 (word String) Engine=URL('https://yandex.ru', CSV)"
        )
        == ""
    )
    assert "not allowed" in node3.query_and_get_error(
        "CREATE TABLE table_test_3_3 (word String) Engine=URL('https://host', CSV)"
    )
    assert "not allowed" in node3.query_and_get_error(
        "CREATE TABLE table_test_3_4 (word String) Engine=URL('https://yandex2.ru', CSV)"
    )
    node3.query("DROP TABLE table_test_3_1")
    node3.query("DROP TABLE table_test_3_2")


def test_config_without_allowed_hosts_section(start_cluster):
    assert (
        node4.query(
            "CREATE TABLE table_test_4_1 (word String) Engine=URL('https://host:80', CSV)"
        )
        == ""
    )
    assert (
        node4.query(
            "CREATE TABLE table_test_4_2 (word String) Engine=S3('https://host:80/bucket/key', CSV)"
        )
        == ""
    )
    assert (
        node4.query(
            "CREATE TABLE table_test_4_3 (word String) Engine=URL('https://host', CSV)"
        )
        == ""
    )
    assert (
        node4.query(
            "CREATE TABLE table_test_4_4 (word String) Engine=URL('https://yandex.ru', CSV)"
        )
        == ""
    )
    assert (
        node4.query(
            "CREATE TABLE table_test_4_5 (word String) Engine=URL('ftp://something.com', CSV)"
        )
        == ""
    )
    node4.query("DROP TABLE table_test_4_1")
    node4.query("DROP TABLE table_test_4_2")
    node4.query("DROP TABLE table_test_4_3")
    node4.query("DROP TABLE table_test_4_4")
    node4.query("DROP TABLE table_test_4_5")


def test_config_without_allowed_hosts(start_cluster):
    assert "not allowed" in node5.query_and_get_error(
        "CREATE TABLE table_test_5_1 (word String) Engine=URL('https://host:80', CSV)"
    )
    assert "not allowed" in node5.query_and_get_error(
        "CREATE TABLE table_test_5_2 (word String) Engine=S3('https://host:80/bucket/key', CSV)"
    )
    assert "not allowed" in node5.query_and_get_error(
        "CREATE TABLE table_test_5_3 (word String) Engine=URL('https://host', CSV)"
    )
    assert "not allowed" in node5.query_and_get_error(
        "CREATE TABLE table_test_5_4 (word String) Engine=URL('https://yandex.ru', CSV)"
    )
    assert "not allowed" in node5.query_and_get_error(
        "CREATE TABLE table_test_5_5 (word String) Engine=URL('ftp://something.com', CSV)"
    )


def test_table_function_remote(start_cluster):
    assert "not allowed in configuration file" not in node6.query_and_get_error(
        "SELECT * FROM remoteSecure('example01-01-{1|2}', system, events)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed in configuration file" not in node6.query_and_get_error(
        "SELECT * FROM remoteSecure('example01-01-1,example01-02-1', system, events)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed in configuration file" not in node6.query_and_get_error(
        "SELECT * FROM remote('example01-0{1,2}-1', system, events",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed in configuration file" not in node6.query_and_get_error(
        "SELECT * FROM remote('example01-0{1,2}-{1|2}', system, events)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed in configuration file" not in node6.query_and_get_error(
        "SELECT * FROM remoteSecure('example01-{01..02}-{1|2}', system, events)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed" in node6.query_and_get_error(
        "SELECT * FROM remoteSecure('example01-01-1,example01-03-1', system, events)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed" in node6.query_and_get_error(
        "SELECT * FROM remote('example01-01-{1|3}', system, events)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert "not allowed" in node6.query_and_get_error(
        "SELECT * FROM remoteSecure('example01-0{1,3}-1', system, metrics)",
        settings={
            "connections_with_failover_max_tries": 1,
            "connect_timeout_with_failover_ms": 1000,
            "connect_timeout_with_failover_secure_ms": 1000,
            "connect_timeout": 1,
            "send_timeout": 1,
        },
    )
    assert node6.query("SELECT * FROM remote('localhost', system, events)") != ""
    assert node6.query("SELECT * FROM remoteSecure('localhost', system, metrics)") != ""
    assert (
        'URL "localhost:800" is not allowed in configuration file'
        in node6.query_and_get_error(
            "SELECT * FROM remoteSecure('localhost:800', system, events)"
        )
    )
    assert (
        'URL "localhost:800" is not allowed in configuration file'
        in node6.query_and_get_error(
            "SELECT * FROM remote('localhost:800', system, metrics)"
        )
    )
