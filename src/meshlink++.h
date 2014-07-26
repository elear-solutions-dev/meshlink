/*
    meshlink++.h -- MeshLink C++ API
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef MESHLINKPP_H
#define MESHLINKPP_H

#include <meshlink.h>

namespace meshlink {
	class mesh;
	class node;
	class channel;

	/// Severity of log messages generated by MeshLink.
	typedef meshlink_log_level_t log_level_t;

	/// Code of most recent error encountered.
	typedef meshlink_errno_t errno_t;

	/// A callback for receiving data from the mesh.
	/** @param mesh      A handle which represents an instance of MeshLink.
	 *  @param source    A pointer to a meshlink::node describing the source of the data.
	 *  @param data      A pointer to a buffer containing the data sent by the source.
	 *  @param len       The length of the received data.
	 */
	typedef void (*receive_cb_t)(mesh *mesh, node *source, const void *data, size_t len);

	/// A callback reporting node status changes.
	/** @param mesh       A handle which represents an instance of MeshLink.
	 *  @param node       A pointer to a meshlink::node describing the node whose status changed.
	 *  @param reachable  True if the node is reachable, false otherwise.
	 */
	typedef void (*node_status_cb_t)(mesh *mesh, node *node, bool reachable);

	/// A callback for receiving log messages generated by MeshLink.
	/** @param mesh      A handle which represents an instance of MeshLink.
	 *  @param level     An enum describing the severity level of the message.
	 *  @param text      A pointer to a string containing the textual log message.
	 */
	typedef void (*log_cb_t)(mesh *mesh, log_level_t level, const char *text);

	/// A callback for accepting incoming channels.
	/** @param mesh         A handle which represents an instance of MeshLink.
	 *  @param channel      A handle for the incoming channel.
	 *  @param node         The node from which this channel is being initiated.
	 *  @param port         The port number the peer wishes to connect to.
	 *  @param data         A pointer to a buffer containing data already received. (Not yet used.)
	 *  @param len          The length of the data. (Not yet used.)
	 *
	 *  @return             This function should return true if the application accepts the incoming channel, false otherwise.
	 *                      If returning false, the channel is invalid and may not be used anymore.
	 */
	typedef bool (*channel_accept_cb_t)(mesh *mesh, channel *channel, node *node, uint16_t port, const void *data, size_t len);

	/// A callback for receiving data from a channel.
	/** @param mesh         A handle which represents an instance of MeshLink.
	 *  @param channel      A handle for the channel.
	 *  @param data         A pointer to a buffer containing data sent by the source.
	 *  @param len          The length of the data.
	 */
	typedef void (*channel_receive_cb_t)(mesh *mesh, channel *channel, const void *data, size_t len);

	/// A class describing a MeshLink node.
	class node: public meshlink_node_t {
	};

	/// A class describing a MeshLink channel.
	class channel: public meshlink_channel_t {
	};

	/// A class describing a MeshLink mesh.
	class mesh: public meshlink_handle_t {
		public:
		// TODO: delete constructor, add a destructor.

		/// Start MeshLink.
		/** This function causes MeshLink to open network sockets, make outgoing connections, and
		 *  create a new thread, which will handle all network I/O.
		 *
		 *  @return         This function will return true if MeshLink has succesfully started its thread, false otherwise.
		 */
		bool start() {
			return meshlink_start(this);
		}

		/// Stop MeshLink.
		/** This function causes MeshLink to disconnect from all other nodes,
		 *  close all sockets, and shut down its own thread.
		 */
		void stop() {
			meshlink_stop(this);
		}

		/// Set the receive callback.
		/** This functions sets the callback that is called whenever another node sends data to the local node.
		 *  The callback is run in MeshLink's own thread.
		 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
		 *  to hand the data over to the application's thread.
		 *  The callback should also not block itself and return as quickly as possible.
		 *
		 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
		 */
		void set_receive_cb(receive_cb_t cb) {
			meshlink_set_receive_cb(this, (meshlink_receive_cb_t)cb);
		}

		/// Set the node status callback.
		/** This functions sets the callback that is called whenever another node's status changed.
		 *  The callback is run in MeshLink's own thread.
		 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
		 *  to hand the data over to the application's thread.
		 *  The callback should also not block itself and return as quickly as possible.
		 *
		 *  @param cb        A pointer to the function which will be called when another node's status changes.
		 */
		void set_node_status_cb(node_status_cb_t cb) {
			meshlink_set_node_status_cb(this, (meshlink_node_status_cb_t)cb);
		}

		/// Set the log callback.
		/** This functions sets the callback that is called whenever MeshLink has some information to log.
		 *  The callback is run in MeshLink's own thread.
		 *  It is important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
		 *  to hand the data over to the application's thread.
		 *  The callback should also not block itself and return as quickly as possible.
		 *
		 *  @param level     An enum describing the minimum severity level. Debugging information with a lower level will not trigger the callback.
		 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
		 */
		void set_log_cb(meshlink_log_level_t level, log_cb_t cb) {
			meshlink_set_log_cb(this, level, (meshlink_log_cb_t)cb);
		}

		/// Send data to another node.
		/** This functions sends one packet of data to another node in the mesh.
		 *  The packet is sent using UDP semantics, which means that
		 *  the packet is sent as one unit and is received as one unit,
		 *  and that there is no guarantee that the packet will arrive at the destination.
		 *  The application should take care of getting an acknowledgement and retransmission if necessary.
		 *
		 *  @param destination  A pointer to a meshlink::node describing the destination for the data.
		 *  @param data         A pointer to a buffer containing the data to be sent to the source.
		 *  @param len          The length of the data.
		 *  @return             This function will return true if MeshLink has queued the message for transmission, and false otherwise.
		 *                      A return value of true does not guarantee that the message will actually arrive at the destination.
		 */
		bool send(node *destination, const void *data, unsigned int len) {
			return meshlink_send(this, destination, data, len);
		}

		/// Get a handle for a specific node.
		/** This function returns a handle for the node with the given name.
		 *
		 *  @param name         The name of the node for which a handle is requested.
		 *
		 *  @return             A pointer to a meshlink::node which represents the requested node,
		 *                      or NULL if the requested node does not exist.
		 */
		node *get_node(const char *name) {
			return (node *)meshlink_get_node(this, name);
		}

		/// Get a list of all nodes.
		/** This function returns a list with handles for all known nodes.
		 *
		 *  @param nodes        A pointer to an array of pointers to meshlink::node, which should be allocated by the application.
		 *  @param nmemb        The maximum number of pointers that can be stored in the nodes array.
		 *
		 *  @return             The number of known nodes. This can be larger than nmemb, in which case not all nodes were stored in the nodes array.
		 */
		size_t get_all_nodes(node **nodes, size_t nmemb) {
			return meshlink_get_all_nodes(this, (meshlink_node_t **)nodes, nmemb);
		}

		/// Sign data using the local node's MeshLink key.
		/** This function signs data using the local node's MeshLink key.
		 *  The generated signature can be securely verified by other nodes.
		 *
		 *  @param data         A pointer to a buffer containing the data to be signed.
		 *  @param len          The length of the data to be signed.
		 *  @param signature    A pointer to a buffer where the signature will be stored.
		 *  @param siglen       The size of the signature buffer. Will be changed after the call to match the size of the signature itself.
		 *
		 *  @return             This function returns true if the signature is valid, false otherwise.
		 */
		bool sign(const void *data, size_t len, void *signature, size_t *siglen) {
			return meshlink_sign(this, data, len, signature, siglen);
		}

		/// Verify the signature generated by another node of a piece of data.
		/** This function verifies the signature that another node generated for a piece of data.
		 *
		 *  @param source       A pointer to a meshlink_node_t describing the source of the signature.
		 *  @param data         A pointer to a buffer containing the data to be verified.
		 *  @param len          The length of the data to be verified.
		 *  @param signature    A pointer to a string containing the signature.
		 *  @param siglen       The size of the signature.
		 *
		 *  @return             This function returns true if the signature is valid, false otherwise.
		 */
		bool verify(node *source, const void *data, size_t len, const void *signature, size_t siglen) {
			return meshlink_verify(this, source, data, len, signature, siglen);
		}

		/// Add an Address for the local node.
		/** This function adds an Address for the local node, which will be used for invitation URLs.
		 *
		 *  @param address      A string containing the address, which can be either in numeric format or a hostname.
		 *
		 *  @return             This function returns true if the address was added, false otherwise.
		 */
		bool add_address(const char *address) {
			return meshlink_add_address(this, address);
		}

		/// Invite another node into the mesh.
		/** This function generates an invitation that can be used by another node to join the same mesh as the local node.
		 *  The generated invitation is a string containing a URL.
		 *  This URL should be passed by the application to the invitee in a way that no eavesdroppers can see the URL.
		 *  The URL can only be used once, after the user has joined the mesh the URL is no longer valid.
		 *
		 *  @param name         The name that the invitee will use in the mesh.
		 *
		 *  @return             This function returns a string that contains the invitation URL.
		 *                      The application should call free() after it has finished using the URL.
		 */
		char *invite(const char *name) {
			return meshlink_invite(this, name);
		}

		/// Use an invitation to join a mesh.
		/** This function allows the local node to join an existing mesh using an invitation URL generated by another node.
		 *  An invitation can only be used if the local node has never connected to other nodes before.
		 *  After a succesfully accepted invitation, the name of the local node may have changed.
		 *
		 *  @param invitation   A string containing the invitation URL.
		 *
		 *  @return             This function returns true if the local node joined the mesh it was invited to, false otherwise.
		 */
		bool join(const char *invitation) {
			return meshlink_join(this, invitation);
		}

		/// Export the local node's key and addresses.
		/** This function generates a string that contains the local node's public key and one or more IP addresses.
		 *  The application can pass it in some way to another node, which can then import it,
		 *  granting the local node access to the other node's mesh.
		 *
		 *  @return             This function returns a string that contains the exported key and addresses.
		 *                      The application should call free() after it has finished using this string.
		 */
		char *export_key() {
			return meshlink_export(this);
		}

		/// Import another node's key and addresses.
		/** This function accepts a string containing the exported public key and addresses of another node.
		 *  By importing this data, the local node grants the other node access to its mesh.
		 *
		 *  @param data         A string containing the other node's exported key and addresses.
		 *
		 *  @return             This function returns true if the data was valid and the other node has been granted access to the mesh, false otherwise.
		 */
		bool import_key(const char *data) {
			return meshlink_import(this, data);
		}

		/// Blacklist a node from the mesh.
		/** This function causes the local node to blacklist another node.
		 *  The local node will drop any existing connections to that node,
		 *  and will not send data to it nor accept any data received from it any more.
		 *
		 *  @param node         A pointer to a meshlink::node describing the node to be blacklisted.
		 */
		void blacklist(node *node) {
			return meshlink_blacklist(this, node);
		}

		/// Set the accept callback.
		/** This functions sets the callback that is called whenever another node sends data to the local node.
		 *  The callback is run in MeshLink's own thread.
		 *  It is therefore important that the callback uses apprioriate methods (queues, pipes, locking, etc.)
		 *  to hand the data over to the application's thread.
		 *  The callback should also not block itself and return as quickly as possible.
		 *
		 *  @param cb        A pointer to the function which will be called when another node sends data to the local node.
		 */
		void set_channel_accept_cb(channel *channel, channel_accept_cb_t cb) {
			return meshlink_set_channel_accept_cb(this, (meshlink_channel_accept_cb_t)cb);
		}

		/// Open a reliable stream channel to another node.
		/** This function is called whenever a remote node wants to open a channel to the local node.
		 *  The application then has to decide whether to accept or reject this channel.
		 *
		 *  @param node         The node to which this channel is being initiated.
		 *  @param port         The port number the peer wishes to connect to.
		 *  @param cb           A pointer to the function which will be called when the remote node sends data to the local node.
		 *  @param data         A pointer to a buffer containing data to already queue for sending.
		 *  @param len          The length of the data.
		 *
		 *  @return             A handle for the channel, or NULL in case of an error.
		 */
		channel *channel_open(node *node, uint16_t port, channel_receive_cb_t recv, const void *data, size_t len) {
			return (channel *)meshlink_channel_open(this, node, port, (meshlink_channel_receive_cb_t)recv, data, len);
		}

		/// Partially close a reliable stream channel.
		/** This shuts down the read or write side of a channel, or both, without closing the handle.
		 *  It can be used to inform the remote node that the local node has finished sending all data on the channel,
		 *  but still allows waiting for incoming data from the remote node.
		 *
		 *  @param channel      A handle for the channel.
		 *  @param direction    Must be one of SHUT_RD, SHUT_WR or SHUT_RDWR.
		 */
		void channel_shutdown(channel *channel, int direction) {
			return meshlink_channel_shutdown(this, channel, direction);
		}

		/// Close a reliable stream channel.
		/** This informs the remote node that the local node has finished sending all data on the channel.
		 *  It also causes the local node to stop accepting incoming data from the remote node.
		 *  Afterwards, the channel handle is invalid and must not be used any more.
		 *
		 *  @param channel      A handle for the channel.
		 */
		void channel_close(meshlink_channel_t *channel) {
			return meshlink_channel_close(this, channel);
		}

		/// Transmit data on a channel
		/** This queues data to send to the remote node.
		 *
		 *  @param channel      A handle for the channel.
		 *  @param data         A pointer to a buffer containing data sent by the source.
		 *  @param len          The length of the data.
		 *
		 *  @return             The amount of data that was queued, which can be less than len, or a negative value in case of an error.
		 */
		ssize_t channel_send(channel *channel, void *data, size_t len) {
			return meshlink_channel_send(this, channel, data, len);
		}

	};

	/// Initialize MeshLink's configuration directory.
	/** This function causes MeshLink to initialize its configuration directory,
	 *  if it hasn't already been initialized.
	 *  It only has to be run the first time the application starts,
	 *  but it is not a problem if it is run more than once, as long as
	 *  the arguments given are the same.
	 *
	 *  This function does not start any network I/O yet. The application should
	 *  first set callbacks, and then call meshlink_start().
	 *
	 *  @param confbase The directory in which MeshLink will store its configuration files.
	 *  @param name     The name which this instance of the application will use in the mesh.
	 *
	 *  @return         This function will return a pointer to a meshlink::mesh if MeshLink has succesfully set up its configuration files, NULL otherwise.
	 */
	static mesh *open(const char *confbase, const char *name) {
		return (mesh *)meshlink_open(confbase, name);
	}

	/// Close the MeshLink handle.
	/** This function calls meshlink_stop() if necessary,
	 *  and frees all memory allocated by MeshLink.
	 *  Afterwards, the handle and any pointers to a struct meshlink_node are invalid.
	 */
	static void close(mesh *mesh) {
		meshlink_close(mesh);
	}
};

#endif // MESHLINKPP_H
