# Paragon Firewall Community Edition
# Copyright (C) 2019-2020  Paragon Software GmbH
#
# This file is part of Paragon Firewall Community Edition.
#
# Paragon Firewall Community Edition is free software: you can
# redistribute it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.
#
# Paragon Firewall Community Edition is distributed in the hope that it
# will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
# the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Paragon Firewall Community Edition. If not, see
#   <https://www.gnu.org/licenses/>.

TERM = ENV['TERM']

def tput(*args)
  if TERM
    `tput #{args.join(' ')}`
  else
    ''
  end
end

TPUT_BOLD = tput('bold')
TPUT_RESET = tput('sgr0')
TPUT_COLS = tput('cols').to_i
TPUT_RED = tput('setaf', '1')
TPUT_GREEN = tput('setaf', '2')

class String
  def tput(attr)
    attr + self + TPUT_RESET
  end

  def bold
    tput(TPUT_BOLD)
  end

  def red
    tput(TPUT_RED)
  end

  def green
    tput(TPUT_GREEN)
  end
end

def msg_good(msg)
  $stderr.puts " #{'*'.green} #{msg}"
end

def msg_bad(msg)
  $stderr.puts " #{'*'.red} #{msg}"
end

class RunError < RuntimeError
  attr_reader :output

  def initialize(message, output)
    super(message)
    @output = output
  end
end

def run(*args)
  chdir = nil
  verbose = ENV['VERBOSE']
  with_output = false

  if args.last.is_a?(Hash)
    args.pop.each do |key, value|
      case key
      when :in then chdir = value
      when :verbose then verbose = value
      when :with_output then with_output = value
      else raise "unknown option #{key} = #{value}"
      end
    end
  end

  run_str = 'run: '
  run_title_len = run_str.length
  run_str = run_str.bold

  cols = (TPUT_COLS > 0 ? TPUT_COLS : 1000) - run_title_len
  cols_used = 0

  args.each do |arg|
    arg_len = arg.length

    if cols_used == 0
      run_str << arg
      cols_used += arg_len
    elsif arg_len + 3 < (cols - cols_used)
      run_str << ' ' + arg
      cols_used += 1 + arg_len
    else
      run_str << " \\\n  " + (' ' * run_title_len) + arg
      cols_used = arg_len + 2
    end
  end

  $stderr.puts run_str
  $stderr.puts " in:".bold + ' ' + chdir if chdir

  out_r, out_w = [nil, nil]
  err_r, err_w = IO.pipe

  system_opts = {}

  if with_output
    out_r, out_w = IO.pipe
    system_opts[:out] = out_w
  elsif !verbose
    system_opts[:out] = err_w
  end

  system_opts[:err] = err_w unless verbose

  system_opts[:chdir] = chdir if chdir

  pid = spawn(*args, system_opts)

  output =
    if with_output
      out_w.close
      out_r.read
    end

  err_w.close
  err_output = err_r.read

  Process.waitpid(pid)

  status = $?
  unless status.success?
    msg_bad "#{'error:'.red} #{args.first.bold} did exit with code #{status.exitstatus}:"
    $stderr.puts output if output && verbose
    $stderr.puts err_output unless verbose
    raise RunError.new("#{args.first} did fail: #{status}", output ? output : err_output)
  end

  output
end
