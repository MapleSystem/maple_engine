#
# Copyright (C) [2021] Futurewei Technologies, Inc. All rights reserved.
#
# OpenArkCompiler is licensed underthe Mulan Permissive Software License v2.
# You can use this software according to the terms and conditions of the MulanPSL - 2.0.
# You may obtain a copy of MulanPSL - 2.0 at:
#
#   https://opensource.org/licenses/MulanPSL-2.0
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
# FIT FOR A PARTICULAR PURPOSE.
# See the MulanPSL - 2.0 for more details.
#

import gdb
import m_datastore
import m_symbol
import m_util
from m_util import MColors
from m_util import gdb_print
import m_debug
import m_frame
import m_info

class MaplePrintCmd(gdb.Command):
    """prints Maple runtime object data
    mprint: prints a Maple object's data:
    mprint <addr-in-hex>: e.g. mprint 0x13085
    """

    def __init__(self):
        gdb.Command.__init__ (self,
                              "mprint",
                              gdb.COMMAND_DATA,
                              gdb.COMPLETE_NONE)

    def invoke(self, args, from_tty):
        self.mprint_func(args, from_tty)

    def usage(self):
        gdb_print ("  mprint      : prints Maple object data")
        gdb_print ("  mprint <addr-in-hex>: e.g. mprint 0x13085")
        gdb_print ("  mprint <variable-name>: e.g. mprint [object_name.]my_var")

    def mprint_func(self, args, from_tty):
        s = str(args)
        try:
            val = gdb.parse_and_eval(s)
            s = hex(int(val))
        except:
            pass

        x = s.split()
        if len(x) == 1: # address or a variable
            # if address: x[0] is list 0x123456
            # if variable: x[0] must not be a number character
            if not x[0][0].isdigit(): # x[0] is a variable
                ### mprint <variable> is only supported for dyncmic language frames
                return self.mprint_func_dync_by_property_name(x[0])
            else:
                try:
                    addr = int(x[0],16)
                except:
                    self.usage()
                    return
        else:
            self.usage()
            return

        # determine if we are running in a dynamic language program or not
        is_dync, frame = m_frame.is_closest_older_maple_frame_dync()
        if is_dync:
            return self.mprint_func_dync_by_addr(addr)
        else:
            return self.mprint_func_static(addr)

    def mprint_func_static(self, addr):
        '''
        Input:
            addr: address in memory. Int
        Output:
            None
        Decription:
            look up all the data at the address, print them
        '''
        ### From here, it is the code for static language frames
        class_name, full_syntax, type_size  = self.get_class_name_syntax(addr)
        if not full_syntax:
            return

        # if it is a array of class obj, or just non-array regular class obj
        if 'array' in full_syntax and class_name[0] == 'L' \
            or 'class' in full_syntax and class_name[0] == 'L' :
            class_list = self.get_class_list(class_name, full_syntax, type_size)

            if not class_list or len(class_list) is 0:
                gdb_print("no class data found")
                return

            self.display_class_data(class_list, addr)

        # if it is an array of primitive type
        elif 'array' in full_syntax and class_name in m_symbol.java_primitive_type_dict:
            ### array is a object that takes 12 bytes per java standard
            self.display_array_primitive_values(addr + 12, full_syntax, type_size)

        return

    def get_class_name_syntax(self, addr):
        """
        for a given stack address, get its class name, class full syntax (description), and
        the object size.

        params:
          addr: int. a stack address

        returns:
          1, class name: string
          2, full_syntax: string. a full description of the object at this address
          3, size: int. the object size
        """

        try:
            buffer = m_util.gdb_exec_to_str('x/gx ' + hex(addr))
        except:
            return None,None,None
        obj_addr = buffer.split(':')[1].strip(' ')

        try:
            buffer = m_util.gdb_exec_to_str('x ' + obj_addr)
        except:
            return None,None,None
        if not '<' in buffer or not '>' in buffer:
            return None,None,None

        buf = buffer.split(':')[0].split()[1]
        class_name = None
        dimension = None
        if buf[0] is '<' and buf[-1] is '>':
            class_name = m_symbol.get_maple_symbol_name(buf[1:-1])
            full_syntax, type_size = m_symbol.get_maple_symbol_full_syntax(buf[1:-1])
        else:
            return None,None,None

        if m_debug.Debug: m_debug.dbg_print("return class_name:", class_name, "full_syntax:", full_syntax, "type_size:", type_size)
        return class_name, full_syntax, type_size

    def get_class_list(self, class_name, full_syntax, type_size):
        """
        for a given class name and its full syntax, find out all the derived classes.
        return a list of class inheritance hierarchy starting from the most base class.

        params:
          class_name: string. class name
          full_syntax: string. description of the class object
          type_size: int. the size of the class object size.

        returns:
          a list of class inheritance hierarchy starting from the most base class
          The item in the list is a dict with the format of
            {
                'class_name': class name, string
                'full_syntax': class desciption. in string. e.g. 'array int Ivar[]'
                'type_size': int.
                'obj_class': obj_class dict retrieved from m_datastore .macros.def cache.
                             This cache has a dict format:
                             {
                                'fields': list of {'name': field name,
                                                   'length': field length,
                                                   'offset': offset from the starting offset of the class.
                                                  }
                                'base_class': base class name. in string.
                                'size': this is actually not the size, it is endding-offset-of-the-class + 1,
                                    meaning next level obj should start from this offset.
                             }
            }
        """

        inherit_list = []
        name = class_name
        count = 0
        while True:
            obj_class_dict = m_datastore.mgdb_rdata.get_class_def(name)
            if m_debug.Debug: m_debug.dbg_print("count=", count, "obj_class_dict=", obj_class_dict)
            if not obj_class_dict:
                return  None
            if count is 0: # only the first class_name, we know its type at runtime
                mfull_syntax = full_syntax
                mtype_size = type_size
            else: # for all the base objects, we only know the name, but not runtime type
                mfull_syntax = 'class'
                mtype_size = 1
            inherit_list = [{'class_name': name, 'full_syntax': mfull_syntax, 'type_size': mtype_size, \
                            'obj_class': obj_class_dict}] + inherit_list
            count += 1
            if obj_class_dict['base_class'] == 'THIS_IS_ROOT':
                break
            else:
                name = obj_class_dict['base_class']

        if m_debug.Debug: m_debug.dbg_print("count=", count, "obj_class_dict=", obj_class_dict)
        for i, v in enumerate(inherit_list):
            if m_debug.Debug: m_debug.dbg_print("  inherit_list #",i, v)
        if m_debug.Debug: m_debug.dbg_print()

        return inherit_list

    def display_class_data(self, class_list, addr):
        level = len(class_list)
        if level is 0:
            gdb_print("no data found")
            return

        '''print head line before printing level'''
        if 'array' in class_list[level-1]['full_syntax']:
            buffer = 'object type: {}'.format(MColors.MP_FSYNTAX + class_list[level-1]['full_syntax'] + MColors.ENDC)
        else:
            buffer = 'object type: {} {}'.format(MColors.MP_FSYNTAX + class_list[level-1]['full_syntax'] + MColors.ENDC,\
                                                 MColors.MP_FSYNTAX + class_list[level-1]['class_name'] + MColors.ENDC)
        gdb_print(buffer)

        value_string = None
        field_count = 1
        for i in range(level):
            if 'array' in class_list[i]['full_syntax']:
                if class_list[i]['obj_class']['base_class'] == "THIS_IS_ROOT":
                    arr_length = self.get_array_length(addr + class_list[i]['obj_class']['size'])
                    buffer = 'level {} {}: length={}'.format(i+1, MColors.MP_CNAME + class_list[i]['full_syntax'] + MColors.ENDC,arr_length)
                    gdb_print(buffer)
                    self.print_array_class_values(addr+class_list[i]['obj_class']['size'])
                else:
                    arr_length = self.get_array_length(addr + class_list[i-1]['obj_class']['size'])
                    buffer = 'level {} {}: length={}'.format(i+1, MColors.MP_CNAME + class_list[i]['full_syntax'] + MColors.ENDC,arr_length)
                    gdb_print(buffer)
                    self.print_array_class_values(addr+class_list[i-1]['obj_class']['size'])
            else:
                buffer = 'level {} {} {}: '.format(i+1, MColors.MP_FSYNTAX + class_list[i]['full_syntax'] + MColors.ENDC,\
                                                    MColors.MP_CNAME + class_list[i]['class_name'] + MColors.ENDC)
                gdb_print(buffer)

                slist = sorted(class_list[i]['obj_class']['fields'], key=lambda x:x['offset'])
                for v in slist:
                    value_string = self.get_value_from_memory(addr, v['offset'],v['length'])
                    if not value_string:
                        buffer ='  #{0:d},off={1:2d},len={2:2d},"{3:<16s}",value=None'.format(field_count, v['offset']\
                                ,v['length'],v['name'])
                    else:
                        buffer ='  #{0:d},off={1:2d},len={2:2d},"{3:<16s}",value={4}'.format(field_count, v['offset']\
                                ,v['length'],v['name'],value_string)
                    gdb_print(buffer)
                    field_count += 1

        # Check, if the last level of the object is a Ljava_2Flang_2FString_3B (String Object), we display its string value
        if class_list[level - 1]['full_syntax'] == 'class' and class_list[level - 1]['class_name'] == 'Ljava_2Flang_2FString_3B':
            if value_string:
                string_object_addr = value_string.split(',')[0].split('hex:')[1]
                if m_debug.Debug: m_debug.dbg_print("string_object_addr in string=", string_object_addr)
                try:
                    string_object_addr = int(string_object_addr, 16)
                except:
                    return
                self.display_single_string_object_values(string_object_addr)

        return

    def get_array_length(self, addr):
        cmd = 'x/1xw ' + hex(addr)
        try:
            buffer = m_util.gdb_exec_to_str(cmd)
        except:
            return 0
        item_num = int(buffer.split(':')[1].strip(),16)
        if m_debug.Debug: m_debug.dbg_print("item_num=", item_num)
        return item_num

    def display_array_char_values(self, addr, type_size, item_num):

        if item_num > 32:
            item_list = [i for i in range(24)]
            item_list = item_list + [ i for i in range(item_num - 8, item_num)]
        else:
            item_list = [i for i in range(item_num)]

        steps = 0
        show_snip = True
        buf = 'String Value: "' + MColors.MP_STR_V
        for i in item_list:
            obj_addr = addr + 4 + type_size * i  # class reference is a pointer, 8 bytes
            cmd = 'x/1hx ' + hex(obj_addr)
            if m_debug.Debug: m_debug.dbg_print("cmd=", cmd)
            try:
                buffer = m_util.gdb_exec_to_str(cmd)
            except:
                buf = '  {}'.format('no-data')
                gdb_print (buf)
                steps += 1
                return

            steps += 1
            v = buffer.split(':')[1].strip()
            buf = buf + int(v, 16).to_bytes(2, byteorder='big').decode("utf-16-be")

            if item_num > 32 and steps >= 24 and show_snip == True:
                buf = buf + '...'
                show_snip = False

        gdb_print (buf + MColors.ENDC + '"')
        return

    def display_single_string_object_values(self, addr):
        """NOTE!!! NOTE!!! NOTE!!!
        call this when we know addr IS a String Object
        string object is Ljava_2Flang_2FString_3B which has a 12 bytes header.
        """
        class_name, full_syntax, type_size  = self.get_class_name_syntax(addr)
        if m_debug.Debug:
            m_debug.dbg_print("class_name=", class_name, "full_syntax=", full_syntax, "type_size=", type_size, "addr=", addr)
        if not full_syntax:
            return
        if full_syntax != "array C[]":
            return

        # get to the data part of String object in Java by addr + 12
        item_num = self.get_array_length(addr + 12)
        self.display_array_char_values(addr+12, type_size, item_num)


    def display_array_primitive_values(self, addr, full_syntax, type_size):
        buffer = 'Object Type: {}'.format(MColors.MP_FSYNTAX + full_syntax + MColors.ENDC)
        gdb_print(buffer)
        item_num = self.get_array_length(addr)
        buffer = 'Level 1 {}: length={}'.format(MColors.MP_FSYNTAX + full_syntax + MColors.ENDC, item_num)
        gdb_print(buffer)

        if item_num > 10:
            item_list = [0,1,2,3,4,5,6, item_num-3,item_num-2,item_num-1]
        else:
            item_list = [i for i in range(item_num)]

        steps = 0
        show_snip = True
        for i in item_list:
            obj_addr = addr + 4 + type_size * i  # class reference is a pointer, 8 bytes
            if type_size == 8:
                cmd = 'x/1gx ' + hex(obj_addr)
            elif type_size == 4:
                cmd = 'x/1xw ' + hex(obj_addr)
            elif type_size == 2:
                cmd = 'x/1xh ' + hex(obj_addr)
            elif type_size == 1:
                cmd = 'x/1xb ' + hex(obj_addr)
            else:
                return
            if m_debug.Debug: m_debug.dbg_print("cmd=", cmd)
            try:
                buffer = m_util.gdb_exec_to_str(cmd)
            except:
                buf = '  [{}] {}'.format(i,'no-data')
                gdb_print (buf)
                steps += 1
                continue

            steps += 1
            v = buffer.split(':')[1].strip()
            v = hex(int(v, 16)) #remove leading 0s. e.g. 0x000123 to 0x123
            if full_syntax == 'array C[]': # display unicode character
                v = v + ", '" + int(v, 16).to_bytes(2, byteorder='big').decode("utf-16-be") + "'"
            buf = '  [{}] {}'.format(i, v)
            gdb_print (buf)

            if item_num > 10 and steps > 6 and show_snip == True:
                gdb_print("  ...")
                show_snip = False

        return

    def print_array_class_values(self, addr):
        """ display 10 items if length is less than 10, or first 7 and last three if length is over 11 """

        item_num = self.get_array_length(addr)

        if item_num > 10:
            item_list = [0,1,2,3,4,5,6, item_num-3,item_num-2,item_num-1]
        else:
            item_list = [i for i in range(item_num)]

        steps = 0
        show_snip = True
        for i in item_list:
            obj_addr = addr + 4 + 8 * i  # class reference is a pointer, 8 bytes
            cmd = 'x/1gx ' + hex(obj_addr)
            if m_debug.Debug: m_debug.dbg_print("cmd=", cmd)
            try:
                buffer = m_util.gdb_exec_to_str(cmd)
            except:
                buf = '  [{}] {}'.format(i,'no-data')
                gdb_print (buf)
                steps += 1
                continue

            steps += 1
            v = buffer.split(':')[1].strip()
            v = hex(int(v, 16)) #remove leading 0s. e.g. 0x000123 to 0x123
            buf = '  [{}] {}'.format(i, v)
            gdb_print (buf)

            if item_num > 10 and steps > 6 and show_snip == True:
                gdb_print("  ...")
                show_snip = False

        return


    def get_value_from_memory(self, addr, offset, length):
        """
        params:
          addr: int
          offset: int
          length: int

        returns:
          value in string
        """

        if m_debug.Debug: m_debug.dbg_print("addr=", addr, " offset=", offset, " length=", length)
        hex_string = None
        doulbe_string = None
        long_string = None
        int_string = None
        float_string = None
        word_string = None
        short_string = None
        byte_string = None
        ret = None
        if length == 8: # 8 byte, could be a long, a 8 byte address ptr, or a double
            cmd = 'x/1gx ' + hex(addr + offset) # cmd to get 8 byte address
            try:
                hex_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            hex_string = hex_string.split(':')[1]
            hex_string = hex(int(hex_string,16))

            cmd = 'x/1gf ' + hex(addr + offset) # cmd to get 8 byte double value
            try:
                double_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            double_string = double_string.split()[1]

            cmd = 'x/1dg ' + hex(addr + offset) # cmd to get 8 byte long value
            try:
                long_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            long_string = long_string.split()[1]

            ret = 'hex:' + hex_string + ',long:' + long_string + ',double:' + double_string
            if m_debug.Debug: m_debug.dbg_print("ret=", ret)
            return ret
        elif length == 4: # 4 byte,could be int, float, hex
            cmd = 'x/1xw ' + hex(addr + offset) # cmd to get 4 byte hex address
            try:
                hex_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            hex_string = hex_string.split()[1]

            cmd = 'x/1dw ' + hex(addr + offset) # cmd to get 4 byte int
            try:
                int_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            int_string = int_string.split()[1]

            cmd = 'x/1fw ' + hex(addr + offset) # cmd to get 4 byte float
            try:
                float_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            float_string = float_string.split()[1]

            ret = 'hex:'+hex_string+',int:'+int_string+',float:'+float_string
            if m_debug.Debug: m_debug.dbg_print("ret=", ret)
            return ret
        elif length == 2: # 2 byte, could be short, hex, 2 character
            cmd = 'x/1xh ' + hex(addr + offset) # cmd to get 2 byte hex address
            try:
                hex_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            hex_string = hex_string.split()[1]

            cmd = 'x/1dh ' + hex(addr + offset) # cmd to get 2 byte short int
            try:
                short_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            short_string = short_string.split()[1]

            cmd = 'x/2b ' + hex(addr + offset) # cmd to get 2 byte characters
            try:
                byte_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            byte_string = byte_string.split(':')[1].strip()

            ret = 'hex:'+hex_string+',short:'+short_string+',byte:'+byte_string
            if m_debug.Debug: m_debug.dbg_print("ret=", ret)
            return ret
        elif length == 1: # 1 byte. could be hex, c
            cmd = 'x/1xb ' + hex(addr + offset) # cmd to get 1 byte hex
            try:
                hex_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            hex_string = hex_string.split()[1]

            cmd = 'x/1cb ' + hex(addr + offset) # cmd to get 1 byte characters
            try:
                byte_string = m_util.gdb_exec_to_str(cmd)
            except:
                if m_debug.Debug: m_debug.dbg_print()
                return None
            byte_string = byte_string.split(':')[1].strip()

            ret = 'hex:'+hex_string+',byte:'+byte_string
            if m_debug.Debug: m_debug.dbg_print("ret=", ret)
            return ret
        else:
            if m_debug.Debug: m_debug.dbg_print()
            return None

    #########################################################
    ### APIs for JS support                               ###
    #########################################################
    def mprint_func_dync_by_property_name(self, var):
        '''
        input:
            var: a property name (e.g a variable name). String
        output:
            None
        return:
            None
        description:
            for a given property name, look up its real address in memory, and then get the data at
            that real address.
        '''

        is_dync, frame = m_frame.is_closest_older_maple_frame_dync()
        if not is_dync:
            gdb_print("Program is not running in a dynamic language mode, mprint property not supported")
            return

        data = m_info.get_all_dync_properties_data()
        if not data or len(data) == 0:
            gdb_print("property data not ready yet")
            return
        if len(data) == 0:
            gdb_print("property data not ready yet")
            return
        match = [x for x in data if x['name'] == var]
        if not match or len(match) == 0:
            buf = "property " + var + " data not available yet"
            gdb_print(buf)
        else:
            buf = MColors.MP_CNAME + "property name    : " + MColors.ENDC + MColors.MP_STR_V + var + MColors.ENDC
            gdb_print(buf)
            buf = MColors.MP_FSYNTAX + "  tag            : " + MColors.ENDC + MColors.MP_STR_V + match[0]['tag'] + MColors.ENDC
            gdb_print(buf)
            if match[0]['tag'] == 'JSTYPE_OBJECT':
                buf = MColors.MP_FSYNTAX + "  value          : " + MColors.ENDC\
                      + MColors.MP_ANAME + "prop_list" + MColors.ENDC + " = " +  MColors.MP_STR_V + match[0]['value']['prop_list'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "prop_index_map" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['prop_index_map'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "prop_string_map" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['prop_string_map'] + MColors.ENDC + "\n"\
                      + "                   "\
                      + MColors.MP_ANAME + "extensible" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['extensible'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "object_class" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['object_class'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "object_type" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['object_type'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "is_builtin" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['is_builtin'] + MColors.ENDC + "\n"\
                      + "                   "\
                      + MColors.MP_ANAME + "proto_is_builtin" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['proto_is_builtin'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "builtin_id" + MColors.ENDC + " = " + MColors.MP_STR_V + match[0]['value']['builtin_id'] + MColors.ENDC + "\n"\
                      + "                   "\
                      + MColors.MP_ANAME + "prototype" + MColors.ENDC + " = {" + MColors.MP_STR_V + match[0]['value']['prototype'] + MColors.ENDC + "}\n"\
                      + "                   "\
                      + MColors.MP_ANAME + "shared" + MColors.ENDC + " = {" + MColors.MP_STR_V + match[0]['value']['shared'] + MColors.ENDC + "}"
            else:
                buf = MColors.MP_FSYNTAX + "  value          : " + MColors.ENDC + MColors.MP_STR_V + match[0]['value'] + MColors.ENDC
            gdb_print(buf)

            buf = MColors.MP_FSYNTAX + "  node addr      : " + MColors.ENDC + MColors.MP_STR_V + match[0]['node_addr'] + MColors.ENDC
            gdb_print(buf)
            buf = MColors.MP_FSYNTAX + "  next addr      : " + MColors.ENDC + MColors.MP_STR_V + match[0]['next'] + MColors.ENDC
            gdb_print(buf)
            buf = MColors.MP_FSYNTAX + "  parent         : " + MColors.ENDC + MColors.MP_STR_V + "Global Object" + MColors.ENDC
            gdb_print(buf)

        return

    def mprint_func_dync_by_addr(self, addr):
        '''
        input:
            addr: a memory address. int
        output:
            None
        return:
            None
        Description:
            For a specified memory address, find out the data type and the data type index.
            For the index, get the data real address, and then we get the jsvalue from the real addrss.
        '''
        if m_debug.Debug: m_debug.dbg_print("addr in int =", addr)
        mtype, v = m_info.get_jstype_value_by_addr(addr)
        if not mtype or not v: # if address specifed is a property node address
            v = m_info.get_prop_list_node_data(str(addr))
            if not v or len(v) == 0:
                buf = MColors.MP_CNAME + hex(addr) + ": " + MColors.ENDC + "value not valid"
                gdb_print(buf)
                return
            elif v['tag'] == "JSTYPE_OBJECT":
                buf = MColors.MP_CNAME + hex(addr) + ": " + MColors.ENDC + MColors.MP_STR_V + v['name'] + MColors.ENDC
                gdb_print(buf)
                buf = MColors.MP_FSYNTAX + "  tag         : " + MColors.ENDC + MColors.MP_STR_V + v['tag'] + MColors.ENDC
                gdb_print(buf)

                buf = MColors.MP_FSYNTAX + "  value       : " + MColors.ENDC\
                      + MColors.MP_ANAME + "prop_list" + MColors.ENDC + " = " +  MColors.MP_STR_V + v['value']['prop_list'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "prop_index_map" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['prop_index_map'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "prop_string_map" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['prop_string_map'] + MColors.ENDC + "\n"\
                      + "                "\
                      + MColors.MP_ANAME + "extensible" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['extensible'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "object_class" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['object_class'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "object_type" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['object_type'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "is_builtin" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['is_builtin'] + MColors.ENDC + "\n"\
                      + "                "\
                      + MColors.MP_ANAME + "proto_is_builtin" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['proto_is_builtin'] + MColors.ENDC\
                      + ", " + MColors.MP_ANAME + "builtin_id" + MColors.ENDC + " = " + MColors.MP_STR_V + v['value']['builtin_id'] + MColors.ENDC + "\n"\
                      + "                "\
                      + MColors.MP_ANAME + "prototype" + MColors.ENDC + " = {" + MColors.MP_STR_V + v['value']['prototype'] + MColors.ENDC + "}\n"\
                      + "                "\
                      + MColors.MP_ANAME + "shared" + MColors.ENDC + " = {" + MColors.MP_STR_V + v['value']['shared'] + MColors.ENDC + "}"
                gdb_print(buf)
            else: # for other JSTYPE properties
                buf = MColors.MP_CNAME + hex(addr) + ": " + MColors.ENDC + MColors.MP_STR_V + v['name'] + MColors.ENDC
                gdb_print(buf)
                buf = MColors.MP_FSYNTAX + "  tag         : " + MColors.ENDC + MColors.MP_STR_V + v['tag'] + MColors.ENDC
                gdb_print(buf)
                buf = MColors.MP_FSYNTAX + "  value       : " + MColors.ENDC + MColors.MP_STR_V + v['value'] + MColors.ENDC
                gdb_print(buf)

            buf = MColors.MP_FSYNTAX + "  node addr   : " + MColors.ENDC + MColors.MP_STR_V + v['node_addr'] + MColors.ENDC
            gdb_print(buf)
            buf = MColors.MP_FSYNTAX + "  next addr   : " + MColors.ENDC + MColors.MP_STR_V + v['next'] + MColors.ENDC
            gdb_print(buf)
            buf = MColors.MP_FSYNTAX + "  parent      : " + MColors.ENDC + MColors.MP_STR_V + "Global Object" + MColors.ENDC
            gdb_print(buf)
            return

        else: # if address specifed is something like function's parameter in mbt command
            buf = MColors.MP_CNAME + hex(addr) + ": " + MColors.ENDC + MColors.MP_FSYNTAX + "type=" + MColors.ENDC + MColors.MP_STR_V + \
                str(mtype) + MColors.ENDC + ","+ MColors.MP_FSYNTAX + " value=" + MColors.ENDC + MColors.MP_STR_V + str(v) + MColors.ENDC
            gdb_print(buf)

        return
