/*
 * MCA bus driver code
 *
 * Abstracted from 3c509.c.
 *
 */

#include "etherboot.h"
#include "dev.h"
#include "io.h"
#include "mca.h"

#define DEBUG_MCA

#undef DBG
#ifdef DEBUG_MCA
#define DBG(...) printf ( __VA_ARGS__ )
#else
#define DBG(...)
#endif

/*
 * Ensure that there is sufficient space in the shared dev_bus
 * structure for a struct pci_device.
 *
 */
DEV_BUS( struct mca_device, mca_dev );
static char mca_magic[0]; /* guaranteed unique symbol */

/*
 * Fill in parameters for an MCA device based on slot number
 *
 */
static int fill_mca_device ( struct mca_device *mca ) {
	unsigned int i;

	/* Make sure motherboard setup is off */
	outb_p ( 0xff, MCA_MOTHERBOARD_SETUP_REG );

	/* Select the slot */
	outb_p ( 0x8 | ( mca->slot & 0xf ), MCA_ADAPTER_SETUP_REG );

	/* Read the POS registers */
	for ( i = 0 ; i < ( sizeof ( mca->pos ) / sizeof ( mca->pos[0] ) ) ;
	      i++ ) {
		mca->pos[i] = inb_p ( MCA_POS_REG ( i ) );
	}

	/* Kill all setup modes */
	outb_p ( 0, MCA_ADAPTER_SETUP_REG );

	DBG ( "MCA slot %d id %hx (%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx)\n",
	      mca->slot, MCA_ID ( mca ),
	      mca->pos[0], mca->pos[1], mca->pos[2], mca->pos[3],
	      mca->pos[4], mca->pos[5], mca->pos[6], mca->pos[7] );

	return 1;
}

/*
 * Obtain a struct mca * from a struct dev *
 *
 * If dev has not previously been used for an MCA device scan, blank
 * out struct mca
 */
struct mca_device * mca_device ( struct dev *dev ) {
	struct mca_device *mca = dev->bus;

	if ( mca->magic != mca_magic ) {
		memset ( mca, 0, sizeof ( *mca ) );
		mca->magic = mca_magic;
	}
	mca->dev = dev;
	return mca;
}

/*
 * Find an MCA device matching the specified driver
 *
 */
int find_mca_device ( struct mca_device *mca, struct mca_driver *driver ) {
	unsigned int i;

	/* Iterate through all possible MCA slots, starting where we
	 * left off/
	 */
	for ( ; mca->slot < MCA_MAX_SLOT_NR ; mca->slot++ ) {
		/* If we've already used this device, skip it */
		if ( mca->already_tried ) {
			mca->already_tried = 0;
			continue;
		}

		/* Fill in device parameters */
		if ( ! fill_mca_device ( mca ) ) {
			continue;
		}

		/* Compare against driver's ID list */
		for ( i = 0 ; i < driver->id_count ; i++ ) {
			struct mca_id *id = &driver->ids[i];

			if ( MCA_ID ( mca ) == id->id ) {
				DBG ( "Device %s (driver %s) matches ID %hx\n",
				      id->name, driver->name, id->id );
				if ( mca->dev ) {
					mca->dev->name = driver->name;
					mca->dev->devid.bus_type
						= MCA_BUS_TYPE;
					mca->dev->devid.vendor_id
						= GENERIC_MCA_VENDOR;
					mca->dev->devid.device_id = id->id;
				}
				mca->already_tried = 1;
				return 1;
			}
		}
	}

	/* No device found */
	mca->slot = 0;
	return 0;
}
