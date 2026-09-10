/*
**	Command & Conquer Renegade(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WWSaveLoad                                                   *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/wwsaveload/editable.h                        $*
 *                                                                                             *
 *                       Author:: Patrick Smith                                                *
 *                                                                                             *
 *                     $Modtime:: 11/02/00 3:34p                                              $*
 *                                                                                             *
 *                    $Revision:: 27                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __EDITABLE_H
#define __EDITABLE_H


#include "always.h"
#include "persist.h"
#include "parameter.h"
#include "simpleparameter.h"
#include "parameterlist.h"
#include "wwdebug.h"

//////////////////////////////////////////////////////////////////////////////////
//
//	EditableClass
//
//////////////////////////////////////////////////////////////////////////////////
class EditableClass : public PersistClass
{
public:

	/////////////////////////////////////////////////////////////////////
	//	Public methods
	//
	//	Note:  These methods can be implemented in derived classes
	// by the DECLARE_EDITABLE macro.
	//
	/////////////////////////////////////////////////////////////////////
	virtual int						Get_Parameter_Count (void) const;
	virtual ParameterClass *	Lock_Parameter (int i);
	virtual void					Unlock_Parameter (int i);
};

/////////////////////////////////////////////////////////////////////
//	Get_Parameter_Count
/////////////////////////////////////////////////////////////////////
inline int
EditableClass::Get_Parameter_Count (void) const
{
	return 0;
}

/////////////////////////////////////////////////////////////////////
//	Get_Parameter
/////////////////////////////////////////////////////////////////////
inline ParameterClass *
EditableClass::Lock_Parameter (int i)
{
	WWASSERT (0);
	return NULL;
}

/////////////////////////////////////////////////////////////////////
//	Set_Parameter
/////////////////////////////////////////////////////////////////////
inline void
EditableClass::Unlock_Parameter (int i)
{
	return ;
}

//#define	PARAM_EDITING_ON
#ifdef	PARAM_EDITING_ON

	//////////////////////////////////////////////////////////////////////////////////
	//
	//	DECLARE_EDITABLE
	//
	//////////////////////////////////////////////////////////////////////////////////
	#define DECLARE_EDITABLE(_class, _parent)										\
	ParameterListClass plist_##_class;												\
	virtual int _class::Get_Parameter_Count(void) const						\
	{																							\
		return plist_##_class.Count () + _parent::Get_Parameter_Count ();	\
	}																							\
	virtual ParameterClass *_class::Lock_Parameter(int i)						\
	{																							\
		if (i < _parent::Get_Parameter_Count()) {									\
			return _parent::Lock_Parameter (i);										\
		}																						\
		return plist_##_class[i - _parent::Get_Parameter_Count()];			\
	}																							\

	//////////////////////////////////////////////////////////////////////////////////
	//
	//	EDITABLE_PARAM
	//
	//	The following macros are used inside the constructor for an editable
	// object to map member data to abstract parameter objects.
	//
	//	Some examples:
	//
	//		To register the player's name (a string):
	//		EDITABLE_PARAM(GameClass, ParameterClass::TYPE_STRING, Name)
	//
	//		To register the player's name using a more descriptive display string:
	//		NAMED_EDITABLE_PARAM(GameClass, ParameterClass::TYPE_STRING, Name, "Player Name")
	//
	//		To register a unit's maximum health
	//		INT_EDITABLE_PARAM(GameClass, MaxHeatlh, 0, 500)
	//
	//		To register a complex variable (such as an enumerated type:
	//		#ifdef	PARAM_EDITING_ON
	//		EnumParameterClass *EDITABLE_PARAM_TMP = new EnumParameterClass(GangID);
	//		EDITABLE_PARAM_TMP->Set_Name ("Gang");
	//		EDITABLE_PARAM_TMP->Add_Value ("GDI", ID_GDI);
	//		EDITABLE_PARAM_TMP->Add_Value ("NOD", ID_NOD);
	//		EDITABLE_PARAM_TMP->Add_Value ("Neutral", ID_NEUTRAL);
	//		GENERIC_EDITABLE_PARAM(param)
	//		#endif
	//
	//////////////////////////////////////////////////////////////////////////////////


	#define EDITABLE_PARAM(_class, type, data)						plist_##_class.Add (&(data), #data, type);
	#define NAMED_EDITABLE_PARAM(_class, type, data, name)		plist_##_class.Add (&(data), name, type);
	
	#define INT_EDITABLE_PARAM(_class, data, min, max) {						\
		IntParameterClass *EDITABLE_PARAM_TMP = new IntParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);													\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }													\

	#define INT_UNITS_PARAM(_class, data, min, max, unitsname) {	   	\
		IntParameterClass *EDITABLE_PARAM_TMP = new IntParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);													\
		EDITABLE_PARAM_TMP->Set_Units_Name(unitsname);											\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }													\

	#define NAMED_INT_UNITS_PARAM(_class,data,min,max,unitsname,name) {	\
		IntParameterClass *EDITABLE_PARAM_TMP = new IntParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);													\
		EDITABLE_PARAM_TMP->Set_Units_Name(unitsname);											\
		EDITABLE_PARAM_TMP->Set_Name(name);															\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }													\

	#define FLOAT_EDITABLE_PARAM(_class, data, min, max) {						\
		FloatParameterClass *EDITABLE_PARAM_TMP = new FloatParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);														\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }														\

	#define FLOAT_UNITS_PARAM(_class, data, min, max, unitsname) {				\
		FloatParameterClass *EDITABLE_PARAM_TMP = new FloatParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);														\
		EDITABLE_PARAM_TMP->Set_Units_Name(unitsname);												\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }	

	#define NAMED_FLOAT_UNITS_PARAM(_class, data, min, max, unitsname,name) {	\
		FloatParameterClass *EDITABLE_PARAM_TMP = new FloatParameterClass( &data, #data);		\
		EDITABLE_PARAM_TMP->Set_Range (min, max);															\
		EDITABLE_PARAM_TMP->Set_Units_Name(unitsname);													\
		EDITABLE_PARAM_TMP->Set_Name(name);																	\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }	

	#define ANGLE_EDITABLE_PARAM(_class, data, min, max) {						\
		AngleParameterClass *EDITABLE_PARAM_TMP = new AngleParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);														\
		EDITABLE_PARAM_TMP->Set_Units_Name ("degrees");												\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }														\

	#define NAMED_ANGLE_EDITABLE_PARAM(_class, data, min, max, name) {		\
		AngleParameterClass *EDITABLE_PARAM_TMP = new AngleParameterClass( &data, #data);	\
		EDITABLE_PARAM_TMP->Set_Range (min, max);														\
		EDITABLE_PARAM_TMP->Set_Units_Name ("degrees");												\
		EDITABLE_PARAM_TMP->Set_Name(name);																\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }														\

//
//	The temporary each *_PARAM macro creates is local to the macro body and
//	never referenced by the caller. Naming it per line stops consecutive
//	invocations shadowing one another.
//
#define EDITABLE_PARAM_JOIN2( a, b )	a##b
#define EDITABLE_PARAM_JOIN( a, b )	EDITABLE_PARAM_JOIN2( a, b )
#define EDITABLE_PARAM_TMP				EDITABLE_PARAM_JOIN( _param_, __LINE__ )

	#define GENERIC_EDITABLE_PARAM(_class, param)	\
		plist_##_class.Add (param);						\

	#define MODEL_DEF_PARAM(_class, data, name) {									\
		ModelDefParameterClass *EDITABLE_PARAM_TMP = new ModelDefParameterClass (&data);	\
		EDITABLE_PARAM_TMP->Set_Name (#data);															\
		EDITABLE_PARAM_TMP->Set_Base_Class (name);														\
		GENERIC_EDITABLE_PARAM(_class, EDITABLE_PARAM_TMP); }

	#define PHYS_DEF_PARAM(_class, data, name) {										\
		PhysDefParameterClass *EDITABLE_PARAM_TMP = new PhysDefParameterClass (&data);		\
		EDITABLE_PARAM_TMP->Set_Name (#data);															\
		EDITABLE_PARAM_TMP->Set_Base_Class (name);														\
		GENERIC_EDITABLE_PARAM(_class, EDITABLE_PARAM_TMP); }

	#define SCRIPT_PARAM(_class, name, params) {													\
		ScriptParameterClass *EDITABLE_PARAM_TMP = new ScriptParameterClass (&name, &params);		\
		EDITABLE_PARAM_TMP->Set_Name (#name);																		\
		GENERIC_EDITABLE_PARAM(_class, EDITABLE_PARAM_TMP); }

	#define SCRIPTLIST_PARAM(_class, name, name_list, param_list) {										\
		ScriptListParameterClass *EDITABLE_PARAM_TMP = new ScriptListParameterClass (&name_list, &param_list);	\
		EDITABLE_PARAM_TMP->Set_Name (name);																							\
		GENERIC_EDITABLE_PARAM(_class, EDITABLE_PARAM_TMP); }

	#define ENUM_PARAM(_class, data, params) {							\
		EnumParameterClass *EDITABLE_PARAM_TMP = new EnumParameterClass (&data);	\
		EDITABLE_PARAM_TMP->Set_Name (#data);												\
		EDITABLE_PARAM_TMP->Add_Values params;												\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }											\

	#define FILENAME_PARAM(_class, data, desc, extension) {						\
		FilenameParameterClass *EDITABLE_PARAM_TMP = new FilenameParameterClass (&data);	\
		EDITABLE_PARAM_TMP->Set_Name (#data);															\
		EDITABLE_PARAM_TMP->Set_Description (desc);													\
		EDITABLE_PARAM_TMP->Set_Extension (extension);												\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }														\

	#define DEFIDLIST_PARAM(_class, data, root_class_id) {							\
		DefIDListParameterClass *EDITABLE_PARAM_TMP = new DefIDListParameterClass (&data);	\
		EDITABLE_PARAM_TMP->Set_Name (#data);																\
		EDITABLE_PARAM_TMP->Set_Class_ID (root_class_id);												\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }															\

	#define CLASSID_DEFIDLIST_PARAM(_class, data, root_class_id, class_id, name) {	\
		DefIDListParameterClass *EDITABLE_PARAM_TMP = new DefIDListParameterClass (&data);	\
		EDITABLE_PARAM_TMP->Set_Name (name);																	\
		EDITABLE_PARAM_TMP->Set_Class_ID (root_class_id);												\
		EDITABLE_PARAM_TMP->Set_Selected_Class_ID (&class_id);											\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }

	#define ZONE_PARAM(_class, data, name) {											\
		ZoneParameterClass *EDITABLE_PARAM_TMP = new ZoneParameterClass (&data);				\
		EDITABLE_PARAM_TMP->Set_Name (name);																\
		GENERIC_EDITABLE_PARAM(_class, EDITABLE_PARAM_TMP); }

	#define PARAM_SEPARATOR(_class, name) {											\
		SeparatorParameterClass *EDITABLE_PARAM_TMP = new SeparatorParameterClass;			\
		EDITABLE_PARAM_TMP->Set_Name (name);																\
		GENERIC_EDITABLE_PARAM(_class, EDITABLE_PARAM_TMP); }

	#define GENERIC_DEFID_PARAM(_class, data, root_class_id) {						\
		GenericDefParameterClass *EDITABLE_PARAM_TMP = new GenericDefParameterClass (&data);	\
		EDITABLE_PARAM_TMP->Set_Class_ID (root_class_id);												\
		EDITABLE_PARAM_TMP->Set_Name (#data);																\
		plist_##_class.Add (EDITABLE_PARAM_TMP); }


#else

	#define DECLARE_EDITABLE(_class, _parent)
	#define EDITABLE_PARAM(_class, type, data)
	#define NAMED_EDITABLE_PARAM(_class, type, data, name)
	#define INT_EDITABLE_PARAM(_class, data, min, max) 						
	#define INT_UNITS_PARAM(_class, data, min, max, unitsname) 	   	
	#define NAMED_INT_UNITS_PARAM(_class,data,min,max,unitsname,name) 	
	#define FLOAT_EDITABLE_PARAM(_class, data, min, max) 						
	#define FLOAT_UNITS_PARAM(_class, data, min, max, unitsname) 				
	#define NAMED_FLOAT_UNITS_PARAM(_class, data, min, max, unitsname,name) 	
	#define ANGLE_EDITABLE_PARAM(_class, data, min, max) 						
	#define NAMED_ANGLE_EDITABLE_PARAM(_class, data, min, max, name) 		
	#define GENERIC_EDITABLE_PARAM(_class, param)	
	#define MODEL_DEF_PARAM(_class, data, name) 									
	#define PHYS_DEF_PARAM(_class, data, name) 										
	#define SCRIPT_PARAM(_class, name, params)
	#define SCRIPTLIST_PARAM(_class, name, name_list, param_list)
	#define ENUM_PARAM(_class, data, params) 							
	#define FILENAME_PARAM(_class, data, desc, extension) 						
	#define DEFIDLIST_PARAM(_class, data, root_class_id) 							
	#define CLASSID_DEFIDLIST_PARAM(_class, data, root_class_id, class_id, name) 	
	#define ZONE_PARAM(_class, data, name) 											
	#define PARAM_SEPARATOR(_class, name) 											
	#define GENERIC_DEFID_PARAM(_class, data, root_class_id) 						

#endif //PARAM_EDITING_ON


#endif //__EDITABLE_H



