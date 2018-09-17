#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

module Err
  require 'set'
  @err_list = Set[]
  class << self
    attr_accessor :err_list
  end
  
  def self.unpack(flag) 
    flag.to_s.tr("_"," ").sub("inv", "invalid").sub("res", "reserved").
                          sub("ind", "indirect").sub("doub", "double").
                          sub("trip", "tripple").sub("blk", "block").
                          sub("ino", "inode").sub("all", "allocated").
                          sub("dup", "duplicate").upcase
  end  
  private_class_method :unpack
  
  module_function

  def block(flag, block, inode, offset)
    type = unpack(flag)
    Err.err_list.add? "#{type} BLOCK #{block} IN INODE #{inode} " \
                      "AT OFFSET #{offset}"
  end

  def unref(block)
    @err_list.add? "UNREFERENCED BLOCK #{block}"
  end

  def free(flag, element)
    type = unpack(flag)
    negative = type[0] == 'U' ? "NOT " : ""
    @err_list.add? "#{type} #{element} #{negative}ON FREELIST" 
  end

  def link(inode, links, linkcount)
    @err_list.add? "INODE #{inode} HAS #{links} LINKS BUT " \
                   "LINKCOUNT IS #{linkcount}"
  end

  def dir(flag, dir_inode, inode, name)
    type = unpack(flag)
    @err_list.add? "DIRECTORY INODE #{dir_inode} NAME '#{name}' "\
                   "#{type} INODE #{inode}"
  end

  def parent(flag, dir_inode, inode, correct_inode)
    type = flag == :self ? "." : ".."
    @err_list.add? "DIRECTORY INODE #{dir_inode} NAME '#{type}' " \
                   "LINK TO INODE #{inode} SHOULD BE #{correct_inode}"
  end
end

  
def check_input(condition, msg_if_not_true)
  correct_usage_str = "Correct usage: /lab3b csv_file"
  unless condition
    STDERR.puts "#{correct_usage_str}\n#{msg_if_not_true}"
    exit 1
  end
end


def scan_header(contents)
  _, blocks, inodes, block_size, inode_size, *_, nonreserved_inode =
    contents.scan(/^SUPERBLOCK.*/).first.split(",").map(&:to_i)

  *_, inode_table_block =
    contents.scan(/^GROUP.*/).first.split(",").map(&:to_i)

  inode_table_size = (inodes * inode_size).fdiv(block_size).ceil
  reserved_blocks = inode_table_block + inode_table_size - 1
  reserved_inodes = nonreserved_inode - 1

  block_hash = Array.new(blocks, :unreferenced)
  inode_hash = Array.new(inodes + 1, :unreferenced)

  # return
  [block_hash, inode_hash, blocks, inodes, reserved_blocks, reserved_inodes]
end


def indirect_sym(flag, level)
  modifier = ["", "_ind", "_doub_ind", "_trip_ind"].fetch(level)
  (flag.to_s << modifier).to_sym
end


def i_block_offset(num)
  (0..11).include?(num) ? 0 : [12, 268, 65804].fetch(num - 12)
end


check_input(ARGV.length == 1, "Invalid number of arguments.")
check_input(File.exists?(filename = ARGV[0]), "Invalid input file.")
File.open(filename, "r") { |f| @contents = f.read }

begin # begin analysis
@block_hash, @inode_hash, @blocks, @inodes, 
@reserved_blocks, @reserved_inodes = scan_header(@contents)

@contents.scan(/^BFREE.*/).each { |line| 
  @block_hash[line[/\d+/].to_i] = :free
}

@contents.scan(/^IFREE.*/).each { |line| 
  @inode_hash[line[/\d+/].to_i] = :free
}

@contents.scan(/^INODE.*/).each do |line|
  e = line.split(",")
  inode = e[1].to_i
  type = e[2].to_sym
  link_count = e[6].to_i

  Err.free(:all_ino, inode) if @inode_hash[inode] == :free
  
  @inode_hash[inode] = [link_count, 0, 0] # _, links, parent

  unless type == :s 
    15.times.each do |num| #direct
      block = e[num + 12].to_i
      level = num > 11 ? num - 11 : 0
      offset = i_block_offset(num)
      unless block == 0
        s_level, s_inode, s_offset = @block_hash[block]
        unless @reserved_blocks < block && block < @blocks
          flag = block <= @reserved_blocks ? :res : :inv
          Err.block(indirect_sym(flag, level), block, inode, offset)
        end
        if @block_hash[block] == :free then
          Err.free(:all_blk, block) 
        elsif @block_hash[block].kind_of?(Array) then
          Err.block(indirect_sym(:dup, level), block, inode, offset)
          Err.block(indirect_sym(:dup, s_level), block, s_inode, s_offset)
        end
        
        @block_hash[block] = [level, inode, i_block_offset(num)]
      end
    end
  end
end

@inode_hash[2][2] = 2 # set root directory
@contents.scan(/^DIRENT.*/).sort.each do |line|
  e = line.split(",")
  dir_inode = e[1].to_i
  inode = e[3].to_i
  name = e.last.gsub("'", "")

  if @inode_hash[inode].is_a?(Symbol) then
    Err.dir(:unall, dir_inode, inode, name)
  elsif inode > @inodes then
    Err.dir(:inv, dir_inode, inode, name)
  else 
    @inode_hash[inode][1] += 1        # links
    if name == "." then
      Err.parent(:self, dir_inode, inode, dir_inode) unless inode == dir_inode
    elsif name == ".." && inode != @inode_hash[dir_inode][2] then
      unless @inode_hash[dir_inode][2] == 0 || 
             @inode_hash[dir_inode][2] == inode then
        Err.parent(:parent, dir_inode, inode, @inode_hash[inode][2])
      end
      @inode_hash[dir_inode][2] = inode
    else
      unless @inode_hash[inode][2] == 0 || 
             @inode_hash[inode][2] == dir_inode then
        Err.parent(:parent, inode, @inode_hash[inode][2], dir_inode)
      end
      @inode_hash[inode][2] = dir_inode # parent
    end
  end
end

@contents.scan(/^INDIRECT.*/).each do |line|
  _, inode, level, offset, block, ref_block = line.split(",").map(&:to_i)
  flag = :dup if @block_hash[ref_block].kind_of?(Array)
  flag = :res if ref_block <= @reserved_blocks
  flag = :inv if ref_block >= @blocks
  unless flag.nil?
    Err.block(indirect_sym(flag, level), ref_block, inode, offset) 
  end

  @block_hash[ref_block] = [level, inode, offset]
end

@block_hash.each_with_index do |val, block|
  Err.unref(block) if block > @reserved_blocks && val == :unreferenced
end

@inode_hash.each_with_index do |val, inode|
  Err.free(:unall_ino, inode) if inode > @reserved_inodes && 
    val == :unreferenced 
  if val.kind_of?(Array)
    linkcount, links, _ = val
    Err.link(inode, links, linkcount) if links != linkcount
  end
end

Err.err_list.each { |msg| puts msg }
exit (Err.err_list.length > 0 ? 2 : 0)

rescue => e # in case any error occurs
STDERR.puts "Error: #{e.message}"
exit 1
end
