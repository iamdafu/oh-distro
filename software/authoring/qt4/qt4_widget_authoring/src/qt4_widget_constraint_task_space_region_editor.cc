#include <QtGui/QGroupBox>
#include <QtGui/QGridLayout>

#include <authoring/qt4_widget_constraint_task_space_region_editor.h>
#include <vector>
#include <string>
#include <sstream>

using namespace std;
using namespace boost;
using namespace urdf;
using namespace affordance;
using namespace authoring;

Qt4_Widget_Constraint_Task_Space_Region_Editor::
Qt4_Widget_Constraint_Task_Space_Region_Editor( Constraint_Task_Space_Region * constraint, 
                                              Model& robotModel,
                                              vector< AffordanceState >& affordanceCollection, 
                                              QWidget * parent ) : QWidget( parent, Qt::Dialog ),
                                                                    _constraint( constraint ),
                                                                    _robot_model( robotModel ),
                                                                    _object_affordances( affordanceCollection ),
                                                                    _list_widget_parent ( new QListWidget( this ) ),
                                                                    _combo_box_child( new QComboBox( this ) ),
                                                                    _combo_box_type( new QComboBox( this ) ),
                                                                    _double_spin_box_ranges( NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES, NULL ),
                                                                    _check_box_ranges( NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES, NULL ) {
  for( unsigned int i = 0; i < NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES; i++ ){
    _double_spin_box_ranges[ i ] = new QDoubleSpinBox( this );
    _check_box_ranges[ i ] = new QCheckBox( this );
  } 

  vector< shared_ptr< Link > > links;
  this->installEventFilter(this);
  
  _robot_model.getLinks( links );
  for( vector< shared_ptr< Link > >::iterator it1 = links.begin(); it1 != links.end(); it1++ ){
    for( map< string, shared_ptr< vector< shared_ptr< Collision > > > >::iterator it2 = (*it1)->collision_groups.begin(); it2 != (*it1)->collision_groups.end(); it2++ ){
      QListWidgetItem * affordance_item = new QListWidgetItem( QString( "%1-%2" ).arg( QString::fromStdString( (*it1)->name ) ).arg( QString::fromStdString( it2->first ) ), _list_widget_parent );
      affordance_item->setFlags( affordance_item->flags() | Qt::ItemIsUserCheckable );
      affordance_item->setCheckState( Qt::Unchecked );
    }
  }

  if (! (_object_affordances.size() > 0) ) {
      _combo_box_child->addItem( QString("need at least one affordance to act as child" ) );
      _combo_box_child->setEnabled(false);
  } else {
    for( vector< AffordanceState >::const_iterator it = _object_affordances.begin(); it != _object_affordances.end(); it++ ){
      _combo_box_child->addItem( QString::fromStdString( it->getName() ) );
    }
  }


  for ( unsigned int i = 0; i < NUM_CONSTRAINT_TASK_SPACE_REGION_CONTACT_TYPES; i++ ) {
    _combo_box_type->addItem( QString::fromStdString( Constraint_Task_Space_Region::contact_type_t_to_std_string( (contact_type_t) i ) ) );
  }

  for( vector< QDoubleSpinBox* >::iterator it = _double_spin_box_ranges.begin(); it != _double_spin_box_ranges.end(); it++ ){
    (*it)->setSuffix( " m" );
    (*it)->setDecimals( 3 );
    (*it)->setRange( -1000.0, 1000.0 );
    (*it)->setSingleStep( 0.01 );
  } 

  for( vector< QCheckBox* >::iterator it = _check_box_ranges.begin(); it != _check_box_ranges.end(); it++ ){
    (*it)->setCheckState( Qt::Checked );
  }
  
  if( _constraint != NULL ){
    _combo_box_type->setCurrentIndex( _constraint->contact_type() );
    setWindowTitle( QString("[%1] - task space region constraint").arg( QString::fromStdString( _constraint->id() ) ) );
    for( vector< string >::iterator it1 = _constraint->parents().begin(); it1 != _constraint->parents().end(); it1++ ){
      for( unsigned int i = 0; i < _list_widget_parent->count(); i++ ){
        if( _list_widget_parent->item( i )->text().toStdString() == (*it1) ){
          _list_widget_parent->item( i )->setCheckState( Qt::Checked );
        }
      }
    }

    if( _constraint->child() != NULL ){
      for( unsigned int i = 0; i < _object_affordances.size(); i++ ){
        if( _object_affordances[ i ].getName() == _constraint->child()->getName() ){
          _combo_box_child->setCurrentIndex( i );
        }
      }
    } else {
      if( !_object_affordances.empty() ){
        _constraint->child() = &(*_object_affordances.begin());
      }
    }

    for( unsigned int i = 0; i < NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES; i++ ){
      _check_box_ranges[ i ]->setCheckState( _constraint->ranges()[ i ].first ? Qt::Checked : Qt::Unchecked );
      _double_spin_box_ranges[ i ]->setEnabled( _constraint->ranges()[ i ].first );
      _double_spin_box_ranges[ i ]->setValue( _constraint->ranges()[ i ].second );
    }
  }

  QGroupBox * parent_group_box = new QGroupBox( "parent (robot link)" );
  QGridLayout * parent_layout = new QGridLayout();
  parent_layout->addWidget( _list_widget_parent );
  parent_group_box->setLayout( parent_layout );

  QGroupBox * child_group_box = new QGroupBox( "child (affordance)" );
  QGridLayout * child_layout = new QGridLayout();
  child_layout->addWidget( _combo_box_child );
  child_group_box->setLayout( child_layout );

  QGroupBox * type_group_box = new QGroupBox( "type" );
  QGridLayout * type_layout = new QGridLayout();
  type_layout->addWidget( _combo_box_type );
  type_group_box->setLayout( type_layout );

  QGroupBox * range_group_box = new QGroupBox( "range" );
  QGridLayout * range_layout = new QGridLayout();
  range_layout->addWidget( new QLabel( QString::fromUtf8( " \u2264 x \u2264 " ), this ), 0, 2 );
  range_layout->addWidget( new QLabel( QString::fromUtf8( " \u2264 y \u2264 " ), this ), 1, 2 );
  range_layout->addWidget( new QLabel( QString::fromUtf8( " \u2264 z \u2264 " ), this ), 2, 2 );
  for( unsigned int i = 0; i < ( NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES/2 ); i++ ){
    range_layout->addWidget( _check_box_ranges[ 2 * i ], i, 0 );
    range_layout->addWidget( _double_spin_box_ranges[ 2 * i ], i, 1 );
    range_layout->addWidget( _double_spin_box_ranges[ 2 * i + 1 ], i, 3 );
    range_layout->addWidget( _check_box_ranges[ 2 * i + 1 ], i, 4 );
    for( unsigned int j = 0; j < 5; j++ ){
      if( range_layout->itemAtPosition( i, j ) != NULL ){
        range_layout->itemAtPosition( i, j )->setAlignment( Qt::AlignCenter );
      }
    }
  }
  
  range_group_box->setLayout( range_layout );
  
  QGridLayout * widget_layout = new QGridLayout( this );
  widget_layout->addWidget( parent_group_box, 0, 0 );
  widget_layout->addWidget( child_group_box, 1, 0 );
  widget_layout->addWidget( type_group_box, 2, 0 );
  widget_layout->addWidget( range_group_box, 3, 0 );
  setLayout( widget_layout );

  connect( _list_widget_parent, SIGNAL( itemChanged( QListWidgetItem* ) ), this, SLOT( _constraint_changed( QListWidgetItem* ) ) );
  connect( _combo_box_child, SIGNAL( currentIndexChanged( int ) ), this, SLOT( _constraint_changed( int ) ) );
  connect( _combo_box_type, SIGNAL( currentIndexChanged( int ) ), this, SLOT( _constraint_changed( int ) ) );

  for( vector< QDoubleSpinBox* >::iterator it = _double_spin_box_ranges.begin(); it != _double_spin_box_ranges.end(); it++ ){
    connect( (*it), SIGNAL( valueChanged( double ) ), this, SLOT( _constraint_changed( double ) ) );
  }   

  for( vector< QCheckBox* >::iterator it = _check_box_ranges.begin(); it != _check_box_ranges.end(); it++ ){
    connect( (*it), SIGNAL( clicked() ), this, SLOT( _constraint_changed() ) );
  }

}

Qt4_Widget_Constraint_Task_Space_Region_Editor::
~Qt4_Widget_Constraint_Task_Space_Region_Editor() {

}

Qt4_Widget_Constraint_Task_Space_Region_Editor::
Qt4_Widget_Constraint_Task_Space_Region_Editor( const Qt4_Widget_Constraint_Task_Space_Region_Editor& other )  : _robot_model( other._robot_model ),
                                                                                                                  _object_affordances( other._object_affordances ),
                                                                                                                  _double_spin_box_ranges( other._double_spin_box_ranges ),
                                                                                                                  _check_box_ranges( other._check_box_ranges ){
}

Qt4_Widget_Constraint_Task_Space_Region_Editor&
Qt4_Widget_Constraint_Task_Space_Region_Editor::
operator=( const Qt4_Widget_Constraint_Task_Space_Region_Editor& other ) {
  _robot_model = other._robot_model;
  _object_affordances = other._object_affordances;
  _double_spin_box_ranges = other._double_spin_box_ranges;
  _check_box_ranges = other._check_box_ranges;
  return (*this);
}

bool
Qt4_Widget_Constraint_Task_Space_Region_Editor::
eventFilter(QObject *object, QEvent *event)
{
  if( event->type() == QEvent::WindowActivate ) {
    emit widget_selected();
  }
  return false;
}

void
Qt4_Widget_Constraint_Task_Space_Region_Editor::
_constraint_changed( void ){
  for( unsigned int i = 0; i < NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES; i++ ){
    _double_spin_box_ranges[ i ]->setEnabled( _check_box_ranges[ i ]->isChecked() );
  }
  if( _constraint != NULL ) {
    // set the child
    _constraint->child() = NULL;
    if( _combo_box_child->currentIndex() < _object_affordances.size() ){
      _constraint->child() = &( _object_affordances[ _combo_box_child->currentIndex() ] );
    }
    // set the parents
    _constraint->parents().clear();
    for( unsigned int i = 0; i < _list_widget_parent->count(); i++ ){
      if( _list_widget_parent->item( i )->checkState() == Qt::Checked ){
        _constraint->parents().push_back( _list_widget_parent->item( i )->text().toStdString() );
      }
    }
    // set the contact type
    _constraint->contact_type() = ( contact_type_t )( _combo_box_type->currentIndex() );
    // set the ranges
    for( unsigned int i = 0; i < NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES; i++ ){
      _constraint->ranges()[ i ].first = _check_box_ranges[ i ]->isChecked();
      _constraint->ranges()[ i ].second = _double_spin_box_ranges[ i ]->value();
    }
  }
  emit description_update( QString::fromStdString( _constraint->description() ) );
  emit widget_selected();
  return;
}

void
Qt4_Widget_Constraint_Task_Space_Region_Editor::
_constraint_changed( int index ){
  _constraint_changed();
  return;
}

void
Qt4_Widget_Constraint_Task_Space_Region_Editor::
_constraint_changed( double value ){
  _constraint_changed();
  _mark_invalid_spin_boxes();
  return;
}

void
Qt4_Widget_Constraint_Task_Space_Region_Editor::
_constraint_changed( QListWidgetItem * item ){
  _constraint_changed();
  return;
}

void
Qt4_Widget_Constraint_Task_Space_Region_Editor::
_mark_invalid_spin_boxes() {
  QString invalid = "QDoubleSpinBox { background-color: #ff0000; color: white }";
  QString valid = "";
  for( unsigned int i = 0; i < NUM_CONSTRAINT_TASK_SPACE_REGION_RANGES/2; i++ ){
    _double_spin_box_ranges[ 2 * i ]->setStyleSheet( ( _double_spin_box_ranges[ 2 * i ]->value() > _double_spin_box_ranges[ 2 * i + 1 ]->value() ) ? invalid : valid );
    _double_spin_box_ranges[ 2 * i + 1 ]->setStyleSheet( ( _double_spin_box_ranges[ 2 * i ]->value() > _double_spin_box_ranges[ 2 * i + 1 ]->value() ) ? invalid : valid );
  }
  return; 
}

void 
Qt4_Widget_Constraint_Task_Space_Region_Editor::
_map_highlighted_parent( int index ) {
/*
  emit highlight_parent_link_by_name( QString::fromStdString( _robot_affordances[ index ].first ) );
*/
}

namespace authoring {
  ostream&
  operator<<( ostream& out,
              const Qt4_Widget_Constraint_Task_Space_Region_Editor& other ) {
    return out;
  }

}
