#include "ext3.h"
#include <sys/types.h>

#ifndef TYPE
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#define TYPE 1
#endif

// Convert Big Endian to Little Endian.
static inline __le32 be2le(__be32 v) { return __be32_to_cpu(v); }
static inline __le16 be2le(__be16 v) { return __be16_to_cpu(v); }
static inline __u8 be2le(__u8 v) { return v; }

static inline __le32 be2le(__s32 const& v) { return be2le(*reinterpret_cast<__be32 const*>(&v)); }


static int block_size_;

union blocknr_vector_type {
	size_t blocknr;		// The must be a size_t in order to align the least significant bit with the least significant bit of blocknr_vector.
	uint32_t* blocknr_vector;

	void push_back(uint32_t blocknr);
	void remove(uint32_t blocknr);
	void erase(void) { if (is_vector()) delete [] blocknr_vector; blocknr = 0; }
	blocknr_vector_type& operator=(std::vector<uint32_t> const& vec);

	bool empty(void) const { return blocknr == 0; }
	// The rest is only valid if empty() returned false.
	bool is_vector(void) const { assert(!empty()); return !(blocknr & 1); }
	uint32_t size(void) const { return is_vector() ? blocknr_vector[0] : 1; }
	uint32_t first_entry(void) const { return is_vector() ? blocknr_vector[1] : (blocknr >> 1); }
	uint32_t operator[](int index) const { assert(index >= 0 && (size_t)index < size()); return (index == 0) ? first_entry() : blocknr_vector[index + 1]; }
};

blocknr_vector_type& blocknr_vector_type::operator=(std::vector<uint32_t> const& vec)
{
	if (!empty())
		erase();
	uint32_t size = vec.size();
	if (size > 0)
	{
		if (size == 1)
			blocknr = (vec[0] << 1) | 1; 
		else
		{
			blocknr_vector = new uint32_t [size + 1]; 
			blocknr_vector[0] = size;
			for (uint32_t i = 0; i < size; ++i)
				blocknr_vector[i + 1] = vec[i];
		}
	}
	return *this;
}

void blocknr_vector_type::push_back(uint32_t bnr)
{
	if (empty())
	{
		assert(bnr);
		blocknr = (bnr << 1) | 1;
	}
	else if (is_vector())
	{
		uint32_t size = blocknr_vector[0] + 1;
		uint32_t* ptr = new uint32_t [size + 1]; 
		ptr[0] = size;
		for (uint32_t i = 1; i < size; ++i)
			ptr[i] = blocknr_vector[i];
		ptr[size] = bnr;
		delete [] blocknr_vector;
		blocknr_vector = ptr;
	}
	else
	{
		uint32_t* ptr = new uint32_t [3];
		ptr[0] = 2;
		ptr[1] = blocknr >> 1;
		ptr[2] = bnr;
		blocknr_vector = ptr;
	}
}

void blocknr_vector_type::remove(uint32_t blknr)
{
	assert(is_vector());
	uint32_t size = blocknr_vector[0];
	int found = 0;
	for (uint32_t j = 1; j <= size; ++j)
		if (blocknr_vector[j] == blknr)
		{
			found = j;
			break;
		}
	assert(found);
	blocknr_vector[found] = blocknr_vector[size];
	blocknr_vector[0] = --size;
	if (size == 1)
	{
		int last_block = blocknr_vector[1];
		delete [] blocknr_vector;
		blocknr = (last_block << 1) | 1;
	}
}

class Descriptor;
static void add_block_descriptor(uint32_t block, Descriptor*);
static void add_block_in_journal_descriptor(Descriptor* descriptor);

enum descriptor_type_nt {
	dt_unknown,
	dt_tag,
	dt_revoke,
	dt_commit
};

std::ostream& operator<<(std::ostream& os, descriptor_type_nt descriptor_type)
{
	switch (descriptor_type)
	{
		case dt_unknown:
			os << "*UNKNOWN*";
			break;
		case dt_tag:
			os << "TAG";
			break;
		case dt_revoke:
			os << "REVOKE";
			break;
		case dt_commit:
			os << "COMMIT";
			break;
	}
	return os;
}

class Descriptor {
	private:
		uint32_t const M_block;			// Block number in the journal.
		uint32_t const M_sequence;
	public:
		Descriptor(uint32_t block, uint32_t sequence) : M_block(block), M_sequence(sequence) { }
		uint32_t block(void) const { return M_block; }
		uint32_t sequence(void) const { return M_sequence; }
		virtual descriptor_type_nt descriptor_type(void) const = 0;
		virtual void print_blocks(void) const = 0;
		virtual void add_block_descriptors(void) = 0;
	protected:
		virtual ~Descriptor() { }
};

class DescriptorTag : public Descriptor {
	private:
		uint32_t M_blocknr;		// Block number on the file system.
		uint32_t M_flags;
	public:
		DescriptorTag(uint32_t block, uint32_t sequence, journal_block_tag_t* block_tag) :
			Descriptor(block, sequence), M_blocknr(be2le(block_tag->t_blocknr)), M_flags(be2le(block_tag->t_flags)) { }
		virtual descriptor_type_nt descriptor_type(void) const { return dt_tag; }
		virtual void print_blocks(void) const;
		virtual void add_block_descriptors(void) { add_block_descriptor(M_blocknr, this); add_block_in_journal_descriptor(this); }
		uint32_t block(void) const { return M_blocknr; }
};

void DescriptorTag::print_blocks(void) const
{
	std::cout << ' ' << Descriptor::block() << '=' << M_blocknr;
	if ((M_flags & (JFS_FLAG_ESCAPE|JFS_FLAG_DELETED)))
	{
		std::cout << '(';
		uint32_t flags = M_flags;
		if ((flags & JFS_FLAG_ESCAPE))
		{
			std::cout << "ESCAPED";
			flags &= ~JFS_FLAG_ESCAPE;
		}
		if (flags)
			std::cout << '|';
		if ((flags & JFS_FLAG_DELETED))
		{
			std::cout << "DELETED";
			flags &= ~JFS_FLAG_DELETED;
		}
		std::cout << ')';
	}
}

class DescriptorRevoke : public Descriptor {
	private:
		std::vector<uint32_t> M_blocks;
	public:
		DescriptorRevoke(uint32_t block, uint32_t sequence, journal_revoke_header_t* revoke_header);
		virtual descriptor_type_nt descriptor_type(void) const { return dt_revoke; }
		virtual void print_blocks(void) const;
		virtual void add_block_descriptors(void);
};

void DescriptorRevoke::add_block_descriptors(void)
{
	for (std::vector<uint32_t>::iterator iter = M_blocks.begin(); iter != M_blocks.end(); ++iter)
		add_block_descriptor(*iter, this);
	add_block_in_journal_descriptor(this);
}

DescriptorRevoke::DescriptorRevoke(uint32_t block, uint32_t sequence, journal_revoke_header_t* revoke_header) : Descriptor(block, sequence)
{
	uint32_t count = be2le(revoke_header->r_count);
	assert(sizeof(journal_revoke_header_t) <= count && count <= (size_t)block_size_);
	count -= sizeof(journal_revoke_header_t);
	assert(count % sizeof(__be32) == 0);
	count /= sizeof(__be32);
	__be32* ptr = reinterpret_cast<__be32*>((unsigned char*)revoke_header + sizeof(journal_revoke_header_t));
	for (uint32_t b = 0; b < count; ++b)
		M_blocks.push_back(be2le(ptr[b]));
}

void DescriptorRevoke::print_blocks(void) const
{
	for (std::vector<uint32_t>::const_iterator iter = M_blocks.begin(); iter != M_blocks.end(); ++iter)
		std::cout << ' ' << *iter;
}


class Transaction {
	private:
		int M_block;
		int M_sequence;
		bool M_committed;
		std::vector<Descriptor*> M_descriptor;
	public:
		void init(int block, int sequence) { M_block = block; M_sequence = sequence; M_committed = false; }
		void set_committed(void) { assert(!M_committed); M_committed = true; }
		void append(Descriptor* descriptor) { M_descriptor.push_back(descriptor); }

		void print_descriptors(void) const;
		int block(void) const { return M_block; }
		int sequence(void) const { return M_sequence; }
		bool committed(void) const { return M_committed; }
		bool contains_tag_for_block(int block);
};

bool Transaction::contains_tag_for_block(int block)
{
	for (std::vector<Descriptor*>::iterator iter = M_descriptor.begin(); iter != M_descriptor.end(); ++iter)
	{
		if ((*iter)->descriptor_type() == dt_tag)
		{
			DescriptorTag& tag(*static_cast<DescriptorTag*>(*iter));
			if (tag.block() == (uint32_t)block)
				return true;
		}
	}
	return false;
}

void Transaction::print_descriptors(void) const
{
	descriptor_type_nt dt = dt_unknown;
	for (std::vector<Descriptor*>::const_iterator iter = M_descriptor.begin(); iter != M_descriptor.end(); ++iter)
	{
		if ((*iter)->descriptor_type() != dt)
		{
			if (dt != dt_unknown)
				std::cout << '\n';
			dt = (*iter)->descriptor_type();
			std::cout << dt << ':';
		}
		(*iter)->print_blocks();
	}
	std::cout << '\n';
}

