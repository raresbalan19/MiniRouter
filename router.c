#include "queue.h"
#include "lib.h"
#include "protocols.h"

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define rtable_entries 100000
#define arptable_entries 10

struct route_table_entry *route_table;
int rtable_length;

struct arp_table_entry *arp_table;
int arptable_length;


struct route_table_entry *get_best_route(uint32_t ip_dest) {
    int left = 0, right = rtable_length - 1;
    struct route_table_entry *best_route = NULL;
	struct route_table_entry *first = &route_table[0];
	
    while (left < right) {
        int mid = (left + right) / 2;
        struct route_table_entry *current = &route_table[mid];
        if ((ip_dest & first->mask) == current->prefix) {
            if (best_route == NULL || ntohl(current->prefix) > ntohl(best_route->prefix) 
			   || (ntohl(current->prefix) == ntohl(best_route->prefix) && ntohl(current->mask) > ntohl(best_route->mask))) {
                best_route = current;
            }
            right = mid; 
        } else if ((ip_dest & first->mask) < current->prefix) {
            right = mid - 1;
        } else if ((ip_dest & first->mask) > current->prefix){
            left = mid + 1;
			if (left == right) {
				mid = left;
				best_route = &route_table[mid];
			}
        }
    }

    return best_route;
}

struct arp_table_entry *get_arp_entry(uint32_t given_ip) {
	for (int i = 0; i < arptable_length; i++) {
		if (arp_table[i].ip == given_ip) {
			return &arp_table[i];
		}
	}
	return NULL;
}

int compare_route_entries(const void *a, const void *b) {
    const struct route_table_entry *entryA = (const struct route_table_entry *)a;
    const struct route_table_entry *entryB = (const struct route_table_entry *)b;

    if (entryA->prefix != entryB->prefix) {
        return (entryA->prefix > entryB->prefix) - (entryA->prefix < entryB->prefix);
    }

    return (entryA->mask < entryB->mask) - (entryA->mask > entryB->mask);
}


int main(int argc, char *argv[])
{
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argc - 2, argv + 2);

	route_table = malloc(sizeof(struct route_table_entry) * rtable_entries);
	DIE(route_table == NULL, "memory");

	arp_table = malloc(sizeof(struct  arp_table_entry) * arptable_entries);
	DIE(arp_table == NULL, "memory");
	
	rtable_length = read_rtable(argv[1], route_table);
	arptable_length = parse_arp_table("arp_table.txt", arp_table);
	
	qsort((void *)route_table, rtable_length, sizeof(struct route_table_entry), compare_route_entries);

	while (1) {

		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) buf;
		/* Note that packets received are in network order,
		any header field which has more than 1 byte will need to be conerted to
		host order. For example, ntohs(eth_hdr->ether_type). The oposite is needed when
		sending a packet on the link, */
		if (ntohs(eth_hdr->ether_type) == 0x800) {
			struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));

			char *interface_ip = get_interface_ip(interface);
			int int_interface_ip = inet_addr(interface_ip);

			if (int_interface_ip == ip_hdr->daddr) {
				continue;
			}

			uint16_t old_checksum = ip_hdr->check;
			ip_hdr->check = 0;
			if (old_checksum != htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)))) {
				printf("Error -> CHECKSUM!\n");
				continue;
			}

			struct route_table_entry *best_route = get_best_route(ip_hdr->daddr);
			if (best_route == NULL) {
				printf("Error -> BEST_ROUTE!\n");
				continue;
			}

			if (ip_hdr->ttl <= 1) {
				printf("TTL expired!\n");
				continue;
			}
			uint8_t old_ttl = ip_hdr->ttl;
			ip_hdr->ttl = old_ttl - 1;

			ip_hdr->check = ~(~old_checksum +  ~((uint16_t)old_ttl) + (uint16_t)ip_hdr->ttl) - 1;

			struct arp_table_entry *new_arp_entry = get_arp_entry(best_route->next_hop);
			if (new_arp_entry == NULL) {
				printf("Error -> ARP_ENTRY!\n");
				continue;
			}

			uint8_t *current_mac = malloc(sizeof(uint8_t) * 6);
			get_interface_mac(interface, current_mac);

			memcpy(eth_hdr->ether_shost, current_mac, 6);
			memcpy(eth_hdr->ether_dhost, new_arp_entry->mac, 6);
		
			send_to_link(best_route->interface, buf, len);

		}
	}
}
