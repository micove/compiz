/*
 * Compiz configuration system library
 *
 * Copyright (C) 2012 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authored By:
 * Sam Spilsbury <sam.spilsbury@canonical.com>
 */

#include <ccs.h>
#include <compizconfig_ccs_setting_value_operators.h>
#include <iostream>

namespace cct = compiz::config::test;

bool
operator== (const CCSSettingColorValue &lhs,
	    const CCSSettingColorValue &rhs)
{
    if (ccsIsEqualColor (lhs, rhs))
	return true;
    return false;
}

std::ostream &
operator<< (std::ostream &os, const CCSSettingColorValue &v)
{
    return os << "Red: " << std::hex << v.color.red << "Blue: " << std::hex << v.color.blue << "Green: " << v.color.green << "Alpha: " << v.color.alpha
       << std::dec << std::endl;
}

bool
operator== (const CCSSettingKeyValue &lhs,
	    const CCSSettingKeyValue &rhs)
{
    if (ccsIsEqualKey (lhs, rhs))
	return true;
    return false;
}

std::ostream &
operator<< (std::ostream &os, const CCSSettingKeyValue &v)
{
    return os << "Keysym: " << v.keysym << " KeyModMask " << std::hex << v.keyModMask << std::dec << std::endl;
}

bool
operator== (const CCSSettingButtonValue &lhs,
	    const CCSSettingButtonValue &rhs)
{
    if (ccsIsEqualButton (lhs, rhs))
	return true;
    return false;
}

std::ostream &
operator<< (std::ostream &os, const CCSSettingButtonValue &v)
{
    return os << "Button " << v.button << "Button Key Mask: " << std::hex << v.buttonModMask << "Edge Mask: " << v.edgeMask << std::dec << std::endl;
}

bool
operator== (const CCSString &lhs,
	    const std::string &rhs)
{
    if (rhs == lhs.value)
	return true;

    return false;
}

bool
operator== (const std::string &lhs,
	    const CCSString &rhs)
{
    return rhs == lhs;
}

bool
operator== (const std::string &rhs,
	    CCSString	      *lhs)
{
    return *lhs == rhs;
}

std::ostream &
operator<< (std::ostream &os, CCSString &string)
{
    os << string.value << std::endl;
    return os;
}

std::ostream & 
cct::DescribeSettingValueTo (std::ostream          &os,
			     CCSSettingType        type,
			     const CCSSettingValue &value)
{
    switch (type)
    {
	case TypeBool:
	    return os << value.value.asBool;
	    break;
	case TypeInt:
	    return os << value.value.asInt;
	    break;
	case TypeFloat:
	    return os << value.value.asFloat;
	    break;
	case TypeString:
	    return os << value.value.asString;
	    break;
	case TypeColor:
	    return os << value.value.asColor;
	    break;
	case TypeAction:
	    return os << "An action value";
	    break;
	case TypeKey:
	    return os << value.value.asKey;
	    break;
	case TypeButton:
	    return os << value.value.asButton;
	    break;
	case TypeEdge:
	    return os << value.value.asEdge;
	    break;
	case TypeBell:
	    return os << value.value.asBell;
	    break;
	case TypeMatch:
	    return os << value.value.asMatch;
	    break;
	case TypeList:
	    return os << value.value.asList;
	    break;
	default:
	    return os << "No available descriptor, type not known";
	    break;
    }

    return os;
}

std::ostream & 
cct::DescribeSettingListTo (std::ostream          &os,
			    CCSSettingType        type,
			    CCSSettingValueList   list)
{
    unsigned int count = 0;
    while (list)
    {
	os << "Item " << count << " ";
	cct::DescribeSettingValueTo (os, type, *list->data);
	list = list->next;
	++count;
    }

    return os;
}

std::ostream &
operator<< (std::ostream &os, const CCSSettingValue &v)
{
    os << "Possible values in CCSSetting :" << std::endl;
    const unsigned int finalType = static_cast <unsigned int> (TypeNum);

    for (unsigned int i = 0; i < finalType; ++i)
    {
	/* We cannot print list values as there's no guarantee
	 * this is actually a list */
	if (static_cast <CCSSettingType> (i) == TypeList)
	    os << "A list value" << std::endl;
	else
	    cct::DescribeSettingValueTo (os, static_cast <CCSSettingType> (i), v) << std::endl;
    }

    return os;
}

std::ostream &
operator<< (std::ostream &os, CCSSettingValueList l)
{
    os << "Possible list values :" << std::endl;
    const unsigned int finalType = static_cast <unsigned int> (TypeNum);

    for (unsigned int i = 0; i < finalType; ++i)
    {
	/* We cannot print list values as there's no guarantee
	 * this is actually a list */
	if (static_cast <CCSSettingType> (i) == TypeList)
	    os << "A list value" << std::endl;
	else
	    cct::DescribeSettingListTo (os, static_cast <CCSSettingType> (i), l) << std::endl;
    }

    return os;
}
