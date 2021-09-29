#!/usr/bin/env python3
from sys import path
from sys import argv

directory = 'Default_20210923_run3/5374_default-new-blueflood-mode6/'
raspis = {'raspi01/', 'raspi02/', 'raspi03/', 'raspi04/', 'raspi05/', 'raspi06/', 'raspi07/', 'raspi08/', 'raspi09/', 
              'raspi10/', 'raspi12/', 'raspi13/', 'raspi14/', 'raspi15/', 'raspi16/', 'raspi17/', 'raspi18/', 'raspi19/', 
              'raspi20/', 'raspi21/'}



def all_raspis_radio():
    for pi in raspis:
        radio_on_time(pi, directory)

def radio_on_time(raspi, directory):

    #paths
    path_to_origin='Logs/'+ directory +'logs/'+ raspi +'log.txt'
    path_to_cleaned='Logs/'+ directory +'logs/'+ raspi +'cleaned.txt'
    path_to_between='Logs/'+ directory +'logs/'+ raspi +'between.txt'
    path_to_data='Logs/'+ directory +'logs/'+ raspi +'data.txt'

    # opens the specified file
    with open(path_to_origin, encoding="iso-8859-14") as p:
        lines = p.readlines()

    # gets the lines containing the string defined in line.find(string)
    file = ""
    for line in lines:
        if(line.find('{rx-')>0):
            file = file + line
        
    # writes file to specified path
    with open(path_to_cleaned,'w') as f_clean:
        f_clean.write(file)

    # make inbetween file
    file3=""
    with open(path_to_cleaned,'r') as f_cleaned:
        l_cleaned = f_cleaned.readlines()
        count = 1
        for line in l_cleaned:
            if(count == (len(l_cleaned)-1)):
                file3 = file3 + line
            count+=1

    new_line = ""
    for elm in file3.split():
        if (elm.startswith('{')):
            new_line = new_line + elm + ', '
        if (elm.endswith(',')):
            new_line = new_line + elm + ' '

    file4=""
    if(len(new_line.split(', '))>5):
        file4 = new_line.split(', ')[5] + ', ' + new_line.split(', ')[6]
    else:
        file4 = "1, 2"

    #write to new file
    with open(path_to_between,'w') as f_between:
        f_between.write(file4)

    file5 = ""
    with open(path_to_between,'r') as f_b:
        l_b = f_b.readlines()
        for line in l_b:
            if(len(line.split())>1):
                cnt = float(line.split(', ')[0])
                tried = float(line.split(', ')[1])
                if(tried != 0):
                    pdr = cnt/tried
                else:
                    pdr = 0
                file5 = file5 + str(pdr*100) + '\n'

    #write to new file
    with open(path_to_data,'w') as f_data:
        f_data.write(file5)

def append_files():
    file_big = ""
    path_to_big_file='Logs/'+ directory +'logs/packet-delivery-rate.txt'
    for raspi in raspis:
        path_to_data='Logs/'+ directory +'logs/'+ raspi +'data.txt'
        with open(path_to_data,'r') as file:
            lines = file.readlines()
            for line in lines:
                file_big = file_big + line
    
    with open(path_to_big_file,'w') as big_file:
        big_file.write(file_big)

if __name__ == "__main__":
        all_raspis_radio()
        append_files()